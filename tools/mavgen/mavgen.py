#!/usr/bin/python
# -*- coding: utf-8 -*-

'''
Generate C++ classes based on MAVLink XML definitions.
'''

usage = '''
%prog [options]
'''

import sys
import os
from optparse import OptionParser
import re
import array

import xml.etree.ElementTree as ET

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), 'lib'))
from minixsv import pyxsval

def Error(msg, exception = None):
    print('ERROR: ' + msg);
    print('===================================================================')
    if exception is None:
        raise Exception(msg)
    else:
        raise

def ValidateInput(schemaPath):
    '''
    Validate input XML file to check if it matches the specified schema.
    '''
    
    global opts
    print('Validating XML...')
    for xmlFile in opts.xmlDef:
        pyxsval.parseAndValidate(xmlFile, xsdFile = opts.schema)

# Map for reserved symbol names replacements
namesFilter = {
   'DEBUG': 'DEBUG_VALUE'
}

def FilterName(name):
    'Re-map generated names to avoid collisions with target environment symbols.'
    if name in namesFilter:
        return namesFilter[name]
    return name

# Map between extension file name and resulted namespace name
namespaceMap = {
    'ardupilotmega': 'apm',
    'sphengineering': 'sph'
}

################################################################################

class MlFile(object):
    '''Protocol definition files.'''

    def __init__(self, path):
        self.path = path
        
        self.name = os.path.basename(path)
        if len(self.name) > 4 and self.name[-4:] == '.xml':
            self.name = self.name[:-4]
        
        if self.name in namespaceMap:
            self.namespace = namespaceMap[self.name]
        elif self.name == 'common':
            # Common definitions go in global namespace 
            self.namespace = None
        else:
            self.namespace = self.name
            
        self.enums = list()
        self.messages = list()
    
class MlEnum(object):
    '''
    Enum definition in MAVLink.
    '''
    
    class Entry(object):
        '''
        Entry definition in MAVLink enum.
        '''            
        
        def __init__(self, name, value = None):
            self.name = FilterName(name)
            self.value = value
            self.description = None
            # Parameters indexed by index value, stored value is string from XML
            self.params = dict()
        
        def AddParameter(self, index, description):
            if index in self.params:
                Error('Duplicated index %d in enum entry %s' % (index, self.name))
            self.params[index] = description
    
    def __init__(self, file, name):
        self.file = file
        file.enums.append(self)
        self.name = FilterName(name)
        self.description = None
        # List of Entry objects
        self.entries = list()
        self.entriesNames = dict()
    
    def AddEntry(self, entry):
        self.entries.append(entry)
        if entry.name in self.entriesNames:
            Error('Conflicting enum entry name: %s (enum %s)' % (entry.name, self.name))
        self.entriesNames[entry.name] = entry
        
    def Merge(self, enum):
        'Merge entries from another enumeration definition'
        for e in enum.entries:
            self.AddEntry(e)
            
    def GetQualifiedName(self):
        if opts.mergeExt:
            return self.name
        else:
            if self.file.namespace is None:
                return self.name
            else:
                return self.file.namespace + '::' + self.name

class MlType(object):
    # ID values for MAVLink types
    CHAR, UINT8, INT8, UINT16, INT16, UINT32, INT32, UINT64, INT64, FLOAT, \
    DOUBLE, UINT8_VERSION = range(0, 12)
    
    names = ['char', 'uint8_t', 'int8_t', 'uint16_t', 'int16_t', 'uint32_t',
             'int32_t', 'uint64_t', 'int64_t', 'float', 'double',
             'uint8_t_mavlink_version']
    
    @staticmethod
    def GetTypeId(name):
        if name not in MlType.names:
            Error('Unknown type name: ' + name)
        return MlType.names.index(name)
    
    @staticmethod
    def _GetSize(id):
        sizes = [1, 1, 1, 2, 2, 4, 4, 8, 8, 4, 8, 1]
        return sizes[id]
    
    def GetSize(self):
        'Get size in bytes of the type'
        return MlType._GetSize(self.id)
    
    def __init__(self, name):
        '''
        Construct type definition from its name.
        '''
        m = re.match('(\\w+)\\[(\\d+)\\]', name)
        if m is None:
            self.id = MlType.GetTypeId(name)
            self.num = None
        else:
            self.id = MlType.GetTypeId(m.group(1))
            self.num = int(m.group(2))
        
    def GetName(self):
        return MlType.names[self.id]
    
    def GetCount(self):
        return self.num if self.num is not None else 1
    
    def GetCppName(self):
        names = ['Char', 'Uint8', 'Int8', 'Uint16', 'Int16', 'Uint32', 'Int32',
                 'Uint64', 'Int64', 'Float', 'Double', 'Uint8_version']
        return names[self.id]
    
    def GetCppId(self):
        ids = ['CHAR', 'UINT8', 'INT8', 'UINT16', 'INT16', 'UINT32', 'INT32',
               'UINT64', 'INT64', 'FLOAT', 'DOUBLE', 'UINT8_VERSION']
        return ids[self.id]
    
    def GetCrcName(self):
        '''
        Get type name suitable for Mavlink CRC extra byte calculation. Currently
        returns the type as-is, except for uint8_t_mavlink_version which is mapped
        to uint8_t as per reference Mavlink generator code.
        '''
        types = ['char', 'uint8_t', 'int8_t', 'uint16_t', 'int16_t', 'uint32_t',
                 'int32_t', 'uint64_t', 'int64_t', 'float', 'double', 'uint8_t']
        return types[self.id]

class MlMessage(object):
    '''
    Message definition in MAVLink
    '''
    
    class Field(object):
        '''
        Field definition in MAVLink message.
        '''
        def __init__(self, name, typeName, description):
            self.name = FilterName(name)
            self.type = MlType(typeName)
            self.description = description

        def GetSize(self):
            # string is no array but string of chars
            if self.type.GetName() == 'char': 
                return self.type.GetCount()
            else:
                return self.type.GetSize() * self.type.GetCount()


    def __init__(self, file, name, id):
        self.file = file
        file.messages.append(self)
        self.name = FilterName(name)
        self.id = id
        self.description = None
        self.fields = list()
        self.extensions_start = None
        self.len_v1 = 0
        self.len_v2 = 0
    
    def AddField(self, name, typeName, description):
        self.fields.append(MlMessage.Field(name, typeName, description))
        
    def GetCppName(self):
        'Get C++ name for the message class.'
        return self.name.lower().capitalize()
    
    def _CalculateCrcExtraByte(self):
        '''
        Calculate a 8-bit checksum of the key fields of a message, so we can detect 
        incompatible XML changes.
        '''
        crc = x25crc(self.name + ' ')
        for i in range(self.extensions_start):
            # Include only base fields (non extensions) in crc.
            f =  self.fields[i]
            self.len_v1 = self.len_v1 + f.type.GetSize() * f.type.GetCount()
            crc.accumulate(f.type.GetCrcName() + ' ')
            crc.accumulate(f.name + ' ')
            if f.type.num is not None:
                crc.accumulate(chr(f.type.num))
        self.crcExtraByte = (crc.crc & 0xFF) ^ (crc.crc >> 8)
        
    def GetQualifiedName(self):
        if opts.mergeExt:
            return self.name
        else:
            if self.file.namespace is None:
                return self.name
            else:
                return self.file.namespace + '::' + self.name

################################################################################

def ParseEnumEntry(e_entry):
    entry = MlEnum.Entry(e_entry.attrib['name'], 
                         eval(e_entry.attrib['value']) if 'value' in e_entry.attrib else None)
    for e in e_entry:
        if e.tag == 'description':
            entry.description = e.text
        elif e.tag == 'param':
            entry.AddParameter(int(e.attrib['index']), e.text)
    return entry

def ParseEnum(file, e_enum):
    enum = MlEnum(file, e_enum.attrib['name'])
    for e in e_enum:
        if e.tag == 'description':
            enum.description = e.text
        elif e.tag == 'entry':
            enum.AddEntry(ParseEnumEntry(e))
    return enum

def ParseEnums(file, e_enums):
    global opts
    
    for e_enum in e_enums:
        if e_enum.tag != 'enum':
            continue
        enum = ParseEnum(file, e_enum)
        name = enum.GetQualifiedName()
        if name in opts.enums:
            opts.enums[name].Merge(enum)
        else:
            opts.enums[name] = enum

def ParseMessage(file, e_msg):
    msg = MlMessage(file, e_msg.attrib['name'], int(e_msg.attrib['id']))
    for e in e_msg:
        if e.tag == 'description':
            msg.description = e.text
        elif e.tag == 'field':
            msg.AddField(e.attrib['name'], e.attrib['type'], e.text)
        elif e.tag == 'extensions':
            msg.extensions_start = len(msg.fields)
            
    if msg.extensions_start is None:
        msg.extensions_start = len(msg.fields)

    # As per Mavlink version 1.0, fields should be sorted based on sizeof(type).
    # do not sort extension fields.
    msg.fields[:msg.extensions_start] = sorted(msg.fields[:msg.extensions_start], key=lambda f: f.type.GetSize(), reverse=True)

    for f in msg.fields:
        msg.len_v2 = msg.len_v2 + f.type.GetSize() * f.type.GetCount()

    msg._CalculateCrcExtraByte()
    return msg

def ParseMessages(file, e_messages):
    global opts
    
    for e_msg in e_messages:
        if e_msg.tag != 'message':
            continue
        msg = ParseMessage(file, e_msg)
        name = msg.GetQualifiedName()
        if name in opts.messages:
            Error('Conflicting message name: %s' % name)
        opts.messages[name] = msg

def ParseInput():
    '''
    Parse XML input into internal representation.
    '''
    
    global opts
    
    print('Parsing XML...')
    
    opts.enums = dict()
    opts.messages = dict()
    opts.files = dict()
    
    for xmlFile in opts.xmlDef:
        if xmlFile in opts.files:
            Error('File already specfied: %s' % xmlFile)
        file = MlFile(xmlFile)
        opts.files[xmlFile] = file
        tree = ET.parse(xmlFile)
        root = tree.getroot()
        
        e_enums = root.find('enums')
        if e_enums is not None:
            ParseEnums(file, e_enums)
        
        e_messages = root.find('messages')
        if e_messages is not None:
            ParseMessages(file, e_messages)
    
    print('%d enums, %d messages' % (len(opts.enums), len(opts.messages)))

################################################################################

def FormatComment(f, text, indent = 0, doc = True):
    '''
    Format comment so that words wrapping is performed and each line is nicely
    prefixed by '*' character. Paragraphs in source text should be split by more
    than one line feed.
    '''
    if len(text) == 0:
        return
    # Maximal length of one line
    maxLen = 72
    indent = '    ' * indent
    
    paragraphs = re.split('\n\n+', text)
    
    f.write(indent + '/*');
    if doc:
        f.write('*')
        
    for paragraph in paragraphs:
        words = re.split('\\s+', paragraph)
        
        curLine = '';
        for word in words:
            if len(word) == 0:
                continue
            if len(curLine) and len(curLine + word) > maxLen:
                f.write(curLine)
                f.write('\n%s *' % indent)
                curLine = ''
            curLine += ' '
            curLine += word
            
        if len(curLine):
            f.write(curLine)
            f.write('\n%s *' % indent)
            curLine = ''
    
    f.write('/\n')

def GenerateEnum(f, enum):
    if enum.file.namespace is not None:
        f.write('namespace %s {\n' % enum.file.namespace)
        
    if enum.description is not None:
        FormatComment(f, enum.description)
    
    f.write('enum %s {\n' % enum.name)
    
    for entry in enum.entries:
        comment = ''
        if entry.description is not None:
            comment += entry.description + '\n'
        for paramName in sorted(entry.params.keys()):
            comment += 'Param %s: %s\n\n' % (paramName, entry.params[paramName])
        FormatComment(f, comment, 1)
        f.write('    ' + entry.name)
        if entry.value is not None:
            f.write(' = %d,\n' % entry.value)
        else:
            f.write(',\n')
    f.write('};\n')
    
    if enum.file.namespace is not None:
        f.write('} /* namespace %s */\n' % enum.file.namespace)
    
    f.write('\n')

def GenerateMessageIdEnum(f):
    FormatComment(f, 'Mavlink message identifier.')
    
    for fileName in opts.files:
        file = opts.files[fileName]
        if file.namespace is not None:
            f.write('namespace %s {\n' % file.namespace)
        f.write('enum MESSAGE_ID: MESSAGE_ID_TYPE {\n')
        
        for msg in file.messages:
            if msg.description is not None:
                FormatComment(f, msg.description, 1)
            f.write('    %s = %d,\n' % (msg.name, msg.id))
        f.write('};\n')
        if file.namespace is not None:
            f.write('} /* namespace %s */\n' % file.namespace)
        f.write('\n')

def GenerateEnums(f):
    FormatComment(f, '''
Copyright (c) 2018, Smart Projects Holdings Ltd
All rights reserved.
See LICENSE file for license details.

!!!Do not edit!!! This file is automatically generated by mavgen.py tool.

Enumerations definitions for MAVLink protocol.
''')
    
    f.write('\n#ifndef _AUTO_MAVLINK_ENUMS_H_\n')
    f.write('#define _AUTO_MAVLINK_ENUMS_H_\n')
    f.write('\nnamespace ugcs {\n')
    f.write('namespace vsm {\n')
    f.write('namespace mavlink {\n\n')
    
    for enumName in opts.enums:
        GenerateEnum(f, opts.enums[enumName])
    GenerateMessageIdEnum(f)
    
    f.write('} /* namespace mavlink */\n')
    f.write('} /* namespace vsm */\n')
    f.write('} /* namespace ugcs */\n')
    f.write('#endif  // _AUTO_MAVLINK_ENUMS_H_\n')

def GenerateMessage(f, msg):
    if msg.description is not None:
        FormatComment(f, msg.description)

    structName = 'Pld_struct_' + msg.name.lower()
    if msg.file.namespace is not None:
        f.write('namespace %s {\n' % msg.file.namespace)
    f.write('namespace internal {\n')
    f.write('struct %s {\n' % structName)

    FormatComment(f, "Reset all fields to UgCS default values", 1)
    f.write('    void\n    Reset();\n')

    FormatComment(f, "Return message size on wire without extensions", 1)
    f.write('    size_t\n    Get_size_v1() const {return %d;}\n' % msg.len_v1)

    FormatComment(f, "Return max message size on wire with extensions", 1)
    f.write('    size_t\n    Get_size_v2() const {return %d;}\n' % msg.len_v2)

    for i in range(len(msg.fields)):
        field = msg.fields[i]
        if (i == msg.extensions_start):
            f.write('\n    /* Message extensions */\n\n')
        if field.description is not None:
            FormatComment(f, field.description, 1)
        if field.type.num is None:
            typeStr = field.type.GetCppName()
        else:
            typeStr = 'Value_array<%s, %d>' % (field.type.GetCppName(), field.type.num)
        f.write('    %s %s;\n' % (typeStr, field.name))
    
    f.write('} __PACKED;\n\n')
    
    f.write('extern mavlink::internal::Field_descriptor pld_desc_%s[%d];\n\n' % (msg.name.lower(), len(msg.fields) + 1))
    
    f.write('extern const char pld_name_%s[%d];\n\n' % (msg.name.lower(), len(msg.name) + 1))
    
    f.write('} /* namespace internal */\n\n')
    
    if msg.file.namespace is None:
        id = 'MESSAGE_ID::%s' % msg.name
    else:
        id = '%s::MESSAGE_ID::%s' % (msg.file.namespace, msg.name)
        
    FormatComment(f, '@see internal::%s' % structName)
    
    f.write(('typedef Payload<internal::%s,\n' + 
             '                internal::pld_desc_%s,\n' +
             '                internal::pld_name_%s,\n' +
             '                %s,\n' +
             '                %d>\n' +
             '    Pld_%s;\n') %
            (structName, msg.name.lower(), msg.name.lower(), id, 
             msg.crcExtraByte, msg.name.lower()))
    
    if msg.file.namespace is not None:
        f.write('} /* namespace %s */\n' % msg.file.namespace)
    
    namespace = '' if msg.file.namespace is None else msg.file.namespace + '::'
    extension = '' if msg.file.namespace is None else ', ' + namespace + 'Extension'
    f.write(
'''
template<>
struct Payload_type_mapper<%s%s> {
    typedef %sPld_%s type;
};


''' % (id, extension, namespace, msg.name.lower()))

def GenerateMessageImpl(f, msg):
    namespace = '' if msg.file.namespace is None else msg.file.namespace + '::'
    
    f.write('mavlink::internal::Field_descriptor mavlink::%sinternal::pld_desc_%s[] = {\n' % 
            (namespace, msg.name.lower()))
    
    for field in msg.fields:
        f.write('{"%s", %s, %d},\n' % (field.name, field.type.GetCppId(), 
                                       1 if field.type.num is None else field.type.num))
    
    f.write('{nullptr, NONE, 0}\n')
    f.write('};\n\n')
    
    f.write('const char mavlink::%sinternal::pld_name_%s[%d] = "%s";\n\n' %
            (namespace, msg.name.lower(), len(msg.name) + 1, msg.name))
    
    f.write('void\nmavlink::%sinternal::Pld_struct_%s::Reset() {\n' %
            (namespace, msg.name.lower()))
    for field in msg.fields:
        f.write('    %s.Reset();\n' % (field.name))
    f.write("}\n\n")
    
def GenerateMessageExtraBytes(f):
    f.write('const std::map<MESSAGE_ID_TYPE, Extra_byte_length_pair> mavlink::Extension::crc_extra_bytes_length_map = {\n')
    
    for msgName in opts.messages:
        msg = opts.messages[msgName]
        if msg.file.namespace is not None:
            continue
        f.write('{MESSAGE_ID::%s,\n    {%d, %d}},\n' % (msg.name, msg.crcExtraByte, msg.len_v1))
    
    f.write('};\n\n')
    
    for fileName in opts.files:
        file = opts.files[fileName]
        if file.namespace is None:
            continue
        extension_class = 'Extension_' + file.namespace + '_type_helper'
        f.write('using %s = ::ugcs::vsm::mavlink::%s::Extension;\n' % (extension_class, file.namespace))
        f.write('const %s %s::instance;\n' % (extension_class, extension_class))
        f.write('const std::map<MESSAGE_ID_TYPE, Extra_byte_length_pair> %s::crc_extra_bytes_length_map = {\n' % extension_class)
        for msg in file.messages:
            structName = 'Pld_struct_' + msg.name.lower()
            f.write('{%s::MESSAGE_ID::%s,\n    {%d, %d}},\n' %
                                                    (file.namespace, 
                                                     msg.name, 
                                                     msg.crcExtraByte,
                                                     msg.len_v1))
        f.write('};\n\n')

def GenerateMsgsHdr(f):
    global opts
    
    FormatComment(f, '''
Copyright (c) 2018, Smart Projects Holdings Ltd
All rights reserved.
See LICENSE file for license details.

!!!Do not edit!!! This file is automatically generated by mavgen.py tool.

Messages definitions for MAVLink protocol.
''')
    
    f.write('\n#ifndef _AUTO_MAVLINK_MESSAGES_H_\n')
    f.write('#define _AUTO_MAVLINK_MESSAGES_H_\n')
    f.write('\n#include <map>\n')
    f.write('#include <string>\n')
    f.write('\nnamespace ugcs {\n')
    f.write('namespace vsm {\n')
    f.write('namespace mavlink {\n\n')
    
    for fileName in opts.files:
        file = opts.files[fileName]
        if file.namespace is None:
            continue
        f.write(
'''
namespace %s {
class Extension: public mavlink::Extension {
public:
    Extension() {}
    static const Extension &
    Get() {
        return instance;
    }

    std::string
    Get_name() const override {
        return "%s";
    }

    const std::map<MESSAGE_ID_TYPE, Extra_byte_length_pair> *
    Get_crc_extra_byte_map() const override {
        return &crc_extra_bytes_length_map;
    }

private:
    static const Extension instance;
    static const std::map<MESSAGE_ID_TYPE, Extra_byte_length_pair> crc_extra_bytes_length_map;
};
} /* namespace %s */

''' % (file.namespace, file.namespace, file.namespace)
        )
        
    for msgId in opts.messages:
        GenerateMessage(f, opts.messages[msgId])
    
    f.write('} /* namespace mavlink */\n')
    f.write('} /* namespace vsm */\n')
    f.write('} /* namespace ugcs */\n')
    f.write('#endif  // _AUTO_MAVLINK_MESSAGES_H_\n')

def GenerateMsgsImpl(f):
    global opts
    
    FormatComment(f, '''
Copyright (c) 2018, Smart Projects Holdings Ltd
All rights reserved.
See LICENSE file for license details.

!!!Do not edit!!! This file is automatically generated by mavgen.py tool.

Messages definitions for MAVLink protocol.
''')
    
    f.write('\n#include <ugcs/vsm/mavlink.h>\n')
    f.write('#include <map>\n')
    f.write('\nusing namespace ugcs::vsm;\n')
    f.write('using namespace ugcs::vsm::mavlink;\n\n')
    
    GenerateMessageExtraBytes(f)
    
    for msgId in opts.messages:
        GenerateMessageImpl(f, opts.messages[msgId])
    
def GenerateCppOutput():
    global opts
    
    print('Generating C++ source files...')
    opts.includeDir = os.path.join(opts.outputDir, 'include')
    if not os.path.exists(opts.includeDir):
        os.mkdir(opts.includeDir)
    opts.headersDir = os.path.join(opts.includeDir, 'ugcs')
    if not os.path.exists(opts.headersDir):
        os.mkdir(opts.headersDir)
    opts.headersDir = os.path.join(opts.headersDir, 'vsm')
    if not os.path.exists(opts.headersDir):
        os.mkdir(opts.headersDir)
    
    with open(os.path.join(opts.headersDir, 'auto_mavlink_enums.h'), 'w') as f:
        GenerateEnums(f)
    with open(os.path.join(opts.headersDir, 'auto_mavlink_messages.h'), 'w') as f:
        GenerateMsgsHdr(f)
    with open(os.path.join(opts.outputDir, 'auto_mavlink_messages.cpp'), 'w') as f:
        GenerateMsgsImpl(f)
        
################################################################################
# Wireshark dissector generator starts here

# 
def Get_enum_from_feld(msg, field):
    d = dict(
             autopilot = "MAV_AUTOPILOT",
             base_mode = "MAV_MODE",
             system_status = "MAV_STATE",
             MISSION_ACKtype = "MAV_MISSION_RESULT",
             HEARTBEATtype = "MAV_TYPE",
             param_type = "MAV_PARAM_TYPE",
             command = "MAV_CMD",
             frame = "MAV_FRAME",
             result = "MAV_RESULT",
             mode = "MAV_MODE",
             nav_mode = "MAV_NAV_MODE",
             mount_mode = "MAV_MOUNT_MODE",
             severity = "MAV_SEVERITY",
             )
    try:
        return d[msg.name + field.name]
    except:
        try:
            return d[field.name]
        except:
            return None

def lua_type(mavlink_type):
    # qnd typename conversion
    if (mavlink_type == 'char'):
        lua_t = 'uint8'
    elif (mavlink_type == 'uint8_t_mavlink_version'):
        lua_t = 'uint8'
    else:
        lua_t = mavlink_type.replace('_t', '')
    return lua_t

def generate_lua_msg_fields(outf, msg):
    for f in msg.fields:
        mtype = f.type.GetName()
        ltype = lua_type(mtype)
        count = f.type.GetCount()
        desc = f.description.replace("\"","\\\"")
        val_array = Get_enum_from_feld(msg, f)

        # string is no array, but string of chars
        if mtype == 'char' and count > 1: 
            count = 1
            ltype = 'string'
        
        for i in range(0,count):
            if count>1: 
                array_text = '[' + str(i) + ']'
                index_text = '_' + str(i)
            else:
                array_text = ''
                index_text = ''

            # 1) wireshark does not support value arrays for 64bit numbers.
            # 2) cannot use description argument of ProtoField call because 
            #    wireshark crashes when mask argument specified.
            if f.type.GetSize() > 4 or val_array is None:
                outf.write("f.{0}_{1}{2} = ProtoField.{3}(\"mavlink_proto.{0}_{1}{2}\", \"{1}{4} ({3})\")\n".format
                       (msg.name, f.name, index_text, ltype, array_text, desc))
            else:                
                outf.write("f.{0}_{1}{2} = ProtoField.{3}(\"mavlink_proto.{0}_{1}{2}\", \"{1}{4} ({3})\", base.DEC, enum_{5})\n".format
                       (msg.name, f.name, index_text, ltype, array_text, val_array))
    outf.write('\n')

def generate_field_dissector(outf, name, field, offset):
    mtype = field.type.GetName()
    count = field.type.GetCount()

    # string is no array but string of chars
    if mtype == 'char': 
        count = 1
    
    # handle arrays, but not strings
    
    for i in range(0, count):
        if count > 1: 
            size = field.type.GetSize()
            index_text = '_' + str(i)
        else:
            size = field.GetSize()
            index_text = ''
        outf.write("""    addle(f.{1}_{0}{3}, buffer, tree, offset, {4}, {2}, len)
""".format(field.name, name, size, index_text, offset + i * size))
    return offset + count * size

def generate_payload_dissector(outf, msg):
    my_field_len = 0
    for f in msg.fields:
        my_field_len = my_field_len + f.GetSize()

    outf.write("""
-- {1}
function myfuncs.dissect_payload_{0}(buffer, tree, offset, len)
""".format(msg.id, msg.name))
    
    offset = 0
    for f in msg.fields:
        offset = generate_field_dissector(outf, msg.name, f, offset)

    outf.write("""    return {0}
end
""".format(my_field_len))
    
def GenerateLuaOutput():
    global opts
    
    print('Generating lua file...')
    f = open(os.path.join(opts.outputDir, 'auto_mavlink_dissector.lua'), 'w')
    if f is None:
        Error('Output directory should be specified')

    f.write('''
-- Do not edit! This file is automatically generated by mavgen.py tool.
-- Wireshark dissector for the MAVLink protocol
-- Usage: Copy this file into your wireshark plugins directory.
--        Detailed instructions here: http://wiki.wireshark.org/Lua

mavlink_proto = Proto("mavlink_proto", "MAVLink protocol")

f = mavlink_proto.fields
f.magic = ProtoField.uint8("mavlink_proto.magic", "Magic value / version", base.HEX)
f.length = ProtoField.uint8("mavlink_proto.length", "Payload length")
f.sequence = ProtoField.uint8("mavlink_proto.sequence", "Packet sequence")
f.sysid = ProtoField.uint8("mavlink_proto.sysid", "System id")
f.compid = ProtoField.uint8("mavlink_proto.compid", "Component id")
f.msgid = ProtoField.uint24("mavlink_proto.msgid", "Message id")
f.crc = ProtoField.uint16("mavlink_proto.crc", "Message CRC", base.HEX)
f.payload = ProtoField.uint24("mavlink_proto.payload", "Payload", base.DEC, messageName)
f.rawheader = ProtoField.bytes("mavlink_proto.rawheader", "Unparsable header fragment")
f.rawpayload = ProtoField.bytes("mavlink_proto.rawpayload", "Unparsable payload")

local myfuncs = {}

function addle(field, buffer, tree, base_offset, offset, size, len)
    if (offset < len) then
        if (len < offset + size) then
            local b = ByteArray.new()
            b:append(buffer(base_offset + offset, len - offset):bytes())
            b:set_size(size)
            tree:add_le(field, ByteArray.tvb(b, "temp buf")(0, size))
        else
            tree:add_le(field, buffer(base_offset + offset, size))
        end
    else
        local b = ByteArray.new()
        b:set_size(size)
        tree:add_le(field, ByteArray.tvb(b, "zero buf")(0, size))
    end
end

''')
        
    print('Generating body fields...')
    f.write("messageName = {\n")
    for msgName in opts.messages:
        f.write("[%d] = '%s',\n" % (opts.messages[msgName].id, msgName))
    f.write("}\n")

    for enumName in opts.enums:
        enu = opts.enums[enumName]
        f.write("enum_%s = {\n" % (enu.name))
        val = 0
        for item in enu.entries:
            # fix up undefined enum values
            if item.value is not None:
                val = item.value 
            f.write("[%s] = '%s',\n" % (val, item.name))
            val = val + 1
        f.write("}\n\n")

    print('Generating message fields...')
    for msgName in opts.messages:
        generate_lua_msg_fields(f, opts.messages[msgName])

    print('Generating message dissectors...')
    for msgName in opts.messages:
        generate_payload_dissector(f, opts.messages[msgName])

    f.write( 
"""
-- dissector function
function mavlink_proto.dissector(buffer,pinfo,tree)
    local offset = 0
    local colinfo = ""
    local frame_count = 0
    local header_len = 0
    local trailer_len = 2

    local version 
    local sequence
    local sysid
    local compid
    local msgid
    local msgid_int

    while offset < buffer:len() do

        -- skip until we have a preamble
        while (offset < buffer:len()) do
            if (buffer(offset, 1):uint() == 0xfe) then
                header_len = 6 -- mavlink1
                if (colinfo == "") then
                    colinfo = "mavlink"
                end
                break
            else if (buffer(offset, 1):uint() == 0xfd) then
                header_len = 10  -- mavlink2
                if (colinfo == "") then
                    colinfo = "mavlink2"
                end
                break
                end
            end
            offset = offset + 1
        end

        if (header_len == 0) then
            break
        end

        -- HEADER ----------------------------------------

        -- we need at least header_len bytes to start dissect mavlink.
        local bytes_remaining = buffer:len() - offset
        if (bytes_remaining < header_len + trailer_len) then
            pinfo.desegment_offset = offset
            pinfo.desegment_len = DESEGMENT_ONE_MORE_SEGMENT
            break
        end

        local length = buffer(offset + 1, 1)
        local payload_size = length:uint()
        local frame_size = header_len + payload_size + trailer_len

        if (bytes_remaining < frame_size) then
            -- buf does not contain all the packet data we need.
            pinfo.desegment_offset = offset
            pinfo.desegment_len = DESEGMENT_ONE_MORE_SEGMENT
            break
        end
        
        if (header_len == 6) then
            -- mavlink1
            version = buffer(offset, 1)
            sequence = buffer(offset + 2, 1)
            sysid = buffer(offset + 3, 1)
            compid = buffer(offset + 4, 1)
            msgid = buffer(offset + 5, 1)
        else
            -- mavlink2
            version = buffer(offset, 1)
            sequence = buffer(offset + 4, 1)
            sysid = buffer(offset + 5, 1)
            compid = buffer(offset + 6, 1)
            msgid = buffer(offset + 7, 3)
        end

        local msgnr = msgid:le_uint()
        local subtree
        
        if (messageName[msgnr]) then
            subtree = tree:add(mavlink_proto, buffer(offset, frame_size), "mavlink " .. messageName[msgnr] .." ("..(msgnr)..")")
        else
            subtree = tree:add(mavlink_proto, buffer(offset, frame_size), "mavlink msg " .. msgnr .." len "..(frame_size)..")")
        end

        local header = subtree:add("Header")
        header:add(f.magic,version)
        header:add(f.length, length)
        header:add(f.sequence, sequence)
        header:add(f.sysid, sysid)
        header:add(f.compid, compid)
        header:add_le(f.msgid, msgid)

        -- BODY ----------------------------------------
        
        -- dynamically call the type-specific payload dissector    
        local dissect_payload_fn = "dissect_payload_"..tostring(msgnr) 
        local fn = myfuncs[dissect_payload_fn]
        local parser_error = false
        
        -- do not stumble into exceptions while trying to parse junk
        if (fn == nil) then
            parser_error = true
        else
            local payload = subtree:add_le(f.payload, buffer(offset + header_len, payload_size), msgnr)
            local len_parsed = fn(buffer, payload, offset + header_len, payload_size)
            colinfo = colinfo .. " " .. messageName[msgnr]
            if (payload_size < len_parsed) and (header_len == 10) then
                payload:add("Payload trimmed by " .. len_parsed - payload_size .. " bytes", buffer(offset, payload_size))
            end
            if (payload_size > len_parsed) then
                payload:add("Payload longer than expected by" .. payload_size - len_parsed .. " bytes", buffer(offset + len_parsed, payload_size - len_parsed))
            end
        end

        if (parser_error) then
            offset = offset + 1
        else
            -- CRC ----------------------------------------
            local crc = buffer(offset + header_len + payload_size, 2)
            subtree:add_le(f.crc, crc)
            offset = offset + frame_size
            frame_count = frame_count + 1
        end
    end

    -- some Wireshark decoration
    if (frame_count) then
        pinfo.cols.protocol = "mavlink_proto"
        pinfo.cols.info = colinfo
    end
end

-- bind protocol dissector to UDP port 14450 used by ArDrone and MissionPlanner 
udp_encap = DissectorTable.get("udp.port")
udp_encap:add(14550, mavlink_proto)

-- bind protocol dissector to TCP  
tcp_encap = DissectorTable.get("tcp.port")
-- port 14555 used by ardupilot emulator script 
tcp_encap:add(14555, mavlink_proto)
-- port 14556 used by ardupilot emulator script 
tcp_encap:add(14556, mavlink_proto)
-- port 14557 used by ardupilot emulator script 
tcp_encap:add(14557, mavlink_proto)
-- port 40115 used by ser2net to capture USB serial traffic 
tcp_encap:add(40115, mavlink_proto)

""")
# Wireshark dissector generator ends here
################################################################################

def FormatPyComment(f, text, indent = 0):
    '''
    Format comment so that words wrapping is performed and each line is nicely
    prefixed by '#' character. Paragraphs in source text should be split by more
    than one line feed.
    '''
    # Maximal length of one line
    maxLen = 72
    indent = '    ' * indent
    
    paragraphs = re.split('\n\n+', text)
        
    for paragraph in paragraphs:
        words = re.split('\\s+', paragraph)
        
        curLine = '';
        for word in words:
            if len(word) == 0:
                continue
            if len(curLine) and len(curLine + word) > maxLen:
                f.write('%s#%s\n' % (indent, curLine))
                curLine = ''
            curLine += ' '
            curLine += word
            
        if len(curLine):
            f.write('%s#%s\n' % (indent, curLine))
            curLine = ''
            
def GeneratePyEnum(f, enum):
    if enum.description is not None:
        FormatPyComment(f, enum.description)
    
    f.write('class %s:\n' % enum.name)
    
    curValue = 0
    for entry in enum.entries:
        comment = ''
        if entry.description is not None:
            comment += entry.description + '\n'
        for paramName in sorted(entry.params.keys()):
            comment += 'Param %s: %s\n\n' % (paramName, entry.params[paramName])
        FormatPyComment(f, comment, 1)
        value = entry.value if entry.value is not None else curValue
        curValue = value + 1
        f.write('    %s = %d\n' % (entry.name, value))
    f.write('\n\n')

def GenerateMessageIdPyEnum(f):
    FormatPyComment(f, 'Mavlink message identifier.')

    f.write('class MESSAGE_ID:\n')
    for msgName in opts.messages:
        message = opts.messages[msgName]
        if message.description is not None:
            FormatPyComment(f, message.description, 1)
        f.write('    %s = %d\n' % (message.name, message.id))
    f.write('\n\n')
    
def GeneratePyEnums(f):
    FormatPyComment(f, 'Common enumerations')
    f.write('\n\n')
    for enumName in opts.enums:
        GeneratePyEnum(f, opts.enums[enumName])
    GenerateMessageIdPyEnum(f)
    # Generate enumerations list
    f.write('enumList = [\n')
    for enumName in opts.enums:
        f.write('    %s,\n' % enumName)
    f.write('    MESSAGE_ID,\n]\n\n')
    
def GeneratePyMessage(f, msg):
    if msg.description is not None:
        FormatPyComment(f, msg.description, 1)
        
    name = msg.name.lower().capitalize()
    
    f.write('    class %s:\n' % name)
    f.write('        name = \'%s\'\n' % msg.name)
    f.write('        id = MESSAGE_ID.%s\n' % msg.name)
    f.write('        crcExtraByte = %d\n' % msg.crcExtraByte)

    for field in msg.fields:
        if field.description is not None:
            FormatPyComment(f, field.description, 2)
        f.write('        fd_%s = MsgFieldDesc(\'%s\', MsgFieldType(MsgFieldType.%s, %d))\n' % 
                (field.name, field.name, field.type.GetCppId(),
                 field.type.num if field.type.num is not None else 1))
        
    fieldsList = ', '.join(map(lambda field: 'fd_' + field.name, msg.fields))
    f.write('        fieldDescs = [%s]\n\n' % fieldsList)
    
def GeneratePyMessages(f):
    f.write(
'''
class MsgFieldType:
    def __init__(self, id, numElements):
        self.id = id
        self.numElements = numElements
        
    # ID values for MAVLink types
    CHAR = 0
    UINT8 = 1
    INT8 = 2
    UINT16 = 3
    INT16 = 4
    UINT32 = 5
    INT32 = 6
    UINT64 = 7
    INT64 = 8
    FLOAT = 9
    DOUBLE = 10
    UINT8_VERSION = 11

class MsgFieldDesc:
    def __init__(self, name, type):
        self.name = name
        self.type = type
        
class Msg:

''')
    for msgName in opts.messages:
        GeneratePyMessage(f, opts.messages[msgName])
    
    f.write('    list = [\n')
    for msgName in opts.messages:
        msg = opts.messages[msgName]
        name = msg.name.lower().capitalize()
        f.write('        %s,\n' % name)
    f.write('        ]\n\n')
    
def GeneratePythonOutput():
    global opts
    
    print('Generating Python source files...')
    with open(os.path.join(opts.outputDir, 'auto_mavlink.py'), 'w') as f:
        FormatPyComment(f,
'''
Do not edit! This file is automatically generated by mavgen.py tool.

Definitions for MAVLink protocol.
''')
        f.write('\n')
        GeneratePyEnums(f)
        GeneratePyMessages(f)

################################################################################

def GenerateOutput():
    global opts
    
    if not os.path.exists(opts.outputDir):
        os.mkdir(opts.outputDir)
    
    if opts.lang == 'C++':
        GenerateCppOutput()
    elif opts.lang == 'Python':
        GeneratePythonOutput()
    elif opts.lang == 'Lua':
        GenerateLuaOutput()
    else:
        raise Exception('Invalid language specified')
        
class x25crc(object):
    '''x25 CRC - based on checksum.h from Mavlink library'''
    def __init__(self, buf=''):
        self.crc = 0xffff
        self.accumulate(buf)

    def accumulate(self, buf):
        '''add in some more bytes'''
        bytes = array.array('B')
        if isinstance(buf, array.array):
            bytes.extend(buf)
        else:
            bytes.fromstring(buf)
        accum = self.crc
        for b in bytes:
            tmp = b ^ (accum & 0xff)
            tmp = (tmp ^ (tmp << 4)) & 0xFF
            accum = (accum >> 8) ^ (tmp << 8) ^ (tmp << 3) ^ (tmp >> 4)
            accum = accum & 0xFFFF
        self.crc = accum

################################################################################

def Main():
    global opts
    
    optParser = OptionParser(usage = usage)
    optParser.add_option('--xml-def', dest = 'xmlDef',
                         metavar = 'XML_DEF',
                         action = 'append',
                         help = 'XML files paths with MAVLink definitions')
    optParser.add_option('--output-dir', dest = 'outputDir',
                         metavar = 'OUTPUT_DIR',
                         help = 'Output directory where C++ files are generated')
    optParser.add_option('--schema', dest = 'schema',
                         metavar = 'SCHEMA',
                         help = 'XML schema file to validate the input. Not validated if not specified.')
    optParser.add_option('--lang', dest = 'lang',
                         metavar = 'LANGUAGE', default = 'C++',
                         help = 'Language to generate definitions for.')
    optParser.add_option('--merge-extensions', dest = 'mergeExt',
                         metavar = 'MERGE_EXT', action = 'store_true',
                         help = 'Merge extensions definitions into one result. ' +
                                'No namespaces generated for the extensions')

    (opts, args) = optParser.parse_args()
    
    if opts.xmlDef is None or len(opts.xmlDef) == 0:
        Error('XML input files should be specified')
        
    if opts.outputDir is None:
        Error('C++ output directory should be specified')
        
    if opts.schema:
        ValidateInput(opts.schema)
    
    ParseInput()
    
    GenerateOutput()

if __name__ == '__main__':
    Main()
