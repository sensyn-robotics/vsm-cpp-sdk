// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file properties.h
 *
 * Java properties set implementation.
 */
#ifndef _PROPERTIES_H_
#define _PROPERTIES_H_

#include <ugcs/vsm/exception.h>
#include <ugcs/vsm/singleton.h>

#include <istream>
#include <map>

namespace ugcs {
namespace vsm {

/** This class represents persistent set of properties which can be stored and
 * loaded in/from any stream. Each property is a key and its value. The format
 * of the text representation and overall concepts are corresponding to Java
 * properties - http://docs.oracle.com/javase/6/docs/api/java/util/Properties.html.
 */
class Properties {
public:
    /** Pointer type. */
    typedef std::shared_ptr<Properties> Ptr;

    /** Base class for all Properties exceptions. */
    VSM_DEFINE_EXCEPTION(Exception);
    /** Thrown when text stream parsing fails. */
    VSM_DEFINE_DERIVED_EXCEPTION(Exception, Parse_exception);
    /** Thrown when a specified key not found. */
    VSM_DEFINE_DERIVED_EXCEPTION(Exception, Not_found_exception);
    /** The value cannot be converted to the specified type from its string
     * representation.
     */
    VSM_DEFINE_DERIVED_EXCEPTION(Exception, Not_convertible_exception);

    /** Get global or create new properties instance. */
    template <typename... Args>
    static Ptr
    Get_instance(Args &&... args)
    {
        return singleton.Get_instance(std::forward<Args>(args)...);
    }

    /** Construct empty properties. */
    Properties();

    /** Load properties from text stream.
     *
     * @param stream Input text stream.
     * @throws Parse_exception if invalid data read from the specified stream.
     */
    void
    Load(std::istream &stream);

    /** Check if the property with the specified key exists. */
    bool
    Exists(const std::string &key) const;

    /** Get string value of the property.
     *
     * @param key Key of the value to retrieve.
     * @return String representation of the value.
     * @throws Not_found_exception Value with specified key is not found.
     */
    std::string
    Get(const std::string &key) const;

    /** Get integer value of the property. Base for the number is automatically
     * detected - values which start by "0x" or "0X" prefix are interpreted as
     * hexadecimal, started by "0" - as octal, all the rest as decimal.
     *
     * @param key Key of the value to retrieve.
     * @return Integer representation of the value.
     * @throws Not_found_exception if the value with specified key is not found.
     * @throws Not_convertible_exception if the value cannot be converted to
     *      integer representation.
     */
    long
    Get_int(const std::string &key) const;

    /** Get floating point number value of the property.
     *
     * @param key Key of the value to retrieve.
     * @return Floating point number representation of the value.
     * @throws Not_found_exception if the value with specified key is not found.
     * @throws Not_convertible_exception if the value cannot be converted to
     *      floating point number representation.
     */
    double
    Get_float(const std::string &key) const;

    /** Set string value of the property. If the key was not existing the new
     * entry is added.
     *
     * @param key Key for the new property.
     * @param value String representation for the property.
     */
    void
    Set(const std::string &key, const std::string &value);

    /** Set integer value of the property. If the key was not existing the new
     * entry is added.
     *
     * @param key Key for the new property.
     * @param value Integer representation for the property.
     */
    void
    Set(const std::string &key, int value);

    /** Set floating point number value of the property. If the key was not
     * existing the new entry is added.
     *
     * @param key Key for the new property.
     * @param value Floating point number representation for the property.
     */
    void
    Set(const std::string &key, double value);

    /** Delete the property with the specified key.
     *
     * @param key Key of the property to delete.
     * @throws Not_found_exception if the value with specified key is not found.
     */
    void
    Delete(const std::string &key);

    /** Store properties into the specified stream. */
    void
    Store(std::ostream &stream);

private:
    /** One property representation. */
    class Property {
    public:
        /** String representation of the property. */
        std::string str_repr;
        /** Integer representation of the property if possible. */
        long int_repr;
        /** Floating point number representation of the property if possible. */
        double float_repr;
        /** Flag indicates that integer representation is valid. */
        bool int_valid:1,
        /** Flag indicates that floating point number representation is valid. */
             float_valid:1;

        /** Construct property from string value. */
        Property(std::string &&value);

        /** Construct property from integer value. */
        Property(long value);

        /** Construct property from floating point value. */
        Property(double value);
    };

    typedef std::map<std::string, Property> Table_type;

    /** Properties table. */
    Table_type table;

public:
    /** Stored properties iterator. */
    class Iterator {
    public:

        /** Construct new iterator which iterates values starting from the
         * given prefix.
         * @param iterator Start from.
         * @param end_iterator End.
         * @param prefix Prefix for filtering.
         * @param separator Path nodes separator.
         */
        Iterator(const Table_type::const_iterator &iterator,
                 const Table_type::const_iterator &end_iterator,
                 const std::string &prefix = std::string(),
                 char separator = '.'):
             table_iterator(iterator),
             table_end(end_iterator),
             prefix(prefix),
             separator(separator)
        {
            _NextProp();
        }

        /** Get the value. */
        std::string
        operator *() const
        {
            if (table_iterator == table_end) {
                VSM_EXCEPTION(ugcs::vsm::Internal_error_exception,
                              "Dereferencing end iterator");
            }
            return table_iterator->first;
        }

        /** Get the value. */
        const std::string *
        operator->() const
        {
            if (table_iterator == table_end) {
                VSM_EXCEPTION(ugcs::vsm::Internal_error_exception,
                              "Dereferencing end iterator");
            }
            return &table_iterator->first;
        }

        /** Equality check method. */
        bool
        operator ==(const Iterator &it) const
        {
            return table_iterator == it.table_iterator;
        }

        /** Non-equality check method. */
        bool
        operator !=(const Iterator &it) const
        {
            return !(*this == it);
        }

        /** Prefix increment operator. */
        void
        operator ++();

        /** Postfix increment operator. */
        void
        operator ++(int)
        {
            ++(*this);
        }

        /** Get component of name with the specified index. Components are
         * separated by separator character specified in Properties::begin()
         * method.
         *
         * @param comp_idx Component index.
         * @return String for component.
         * @throws Invalid_param_exception if index is out of range.
         */
        std::string
        operator[](size_t comp_idx);

    private:
        Table_type::const_iterator table_iterator, table_end;
        std::string prefix;
        char separator;

        /** Find next matching property name starting from the current position. */
        void
        _NextProp();
    };

    /** Begin properties iteration.
     *
     * @param prefix Prefix to match property name with.
     * @param separator Separator used for property name components separation.
     * @return Iterator object.
     */
    Iterator
    begin(const std::string &prefix = std::string(), char separator = '.') const
    {
        return Iterator(table.begin(), table.end(), prefix, separator);
    }

    /** Iterator for one-past-the-end position. */
    Iterator
    end() const
    {
        return Iterator(table.end(), table.end());
    }

private:
    /** Singleton object. */
    static Singleton<Properties> singleton;

    /** Find property in a table, throw Not_found_exception if not found. */
    const Property &
    Find_property(const std::string &key) const;

    /** Find property in a table, return nullptr if not found. */
    Property *
    Find_property(const std::string &key);

    /** Escape string. */
    static std::string
    Escape(const std::string &str, bool is_key = false);
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _PROPERTIES_H_ */
