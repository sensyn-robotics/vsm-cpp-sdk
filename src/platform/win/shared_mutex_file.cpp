// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * shared_mutex_file.cpp
 *
 *  Created on: Dec 16, 2013
 *      Author: janis
 */

#include <ugcs/vsm/shared_mutex_file.h>
#include <aclapi.h>

namespace ugcs {
namespace vsm {

Shared_mutex_file::Shared_mutex_file(const std::string& name, File_processor::Ptr fp)
{
    char dest[0x1000];
    ExpandEnvironmentStrings("%SystemRoot%\\Temp\\vsm_shared_mutex_", dest, sizeof(dest));
    std::string mutex_name = dest + name;
    stream = fp->Open(mutex_name, "rx");

    // We need to set the permissions of the mutex file to world-readable
    // It should be accessible for read for any eventual user of this file.
    // This is somewhat involved in windows case...

    PACL old_acl = NULL;
    PACL new_acl = NULL;
    PSID sid_everyone;
    EXPLICIT_ACCESS ea;
    PSECURITY_DESCRIPTOR pSD = NULL;
    SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;

    auto ret = GetNamedSecurityInfo(
            const_cast<LPTSTR>(mutex_name.c_str()),
            SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION,
            NULL,
            NULL,
            &old_acl,
            NULL,
            &pSD);
    if (ret != ERROR_SUCCESS) {
        VSM_SYS_EXCEPTION("Could not get permissions. ret=%d", mutex_name.c_str(), ret);
    }

    // Create a well-known SID for the Everyone group.
    if(!AllocateAndInitializeSid(&SIDAuthWorld, 1,
                     SECURITY_WORLD_RID,
                     0, 0, 0, 0, 0, 0, 0,
                     &sid_everyone)) {
        VSM_SYS_EXCEPTION("AllocateAndInitializeSid");
    }
    // Initialize an EXPLICIT_ACCESS structure for an ACE.
    // The ACE will allow Everyone read access to the key.
    ZeroMemory(&ea, sizeof(ea));
    ea.grfAccessPermissions = FILE_GENERIC_READ;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance= NO_INHERITANCE;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea.Trustee.ptstrName  = reinterpret_cast<LPTSTR>(sid_everyone);

    // Create a new ACL that contains the new ACEs.
    if (ERROR_SUCCESS != SetEntriesInAcl(1, &ea, old_acl, &new_acl)) {
        VSM_SYS_EXCEPTION("SetEntriesInAcl");
    }

    // Do not care about success here. This call will fail if current user
    // is restricted but file was created by privileged user.
    SetNamedSecurityInfo(
            const_cast<LPTSTR>(mutex_name.c_str()),
            SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION,
            NULL,
            NULL,
            new_acl,
            NULL);
    LocalFree(pSD);
    LocalFree(new_acl);
    FreeSid(sid_everyone);
}

} /* namespace vsm */
} /* namespace ugcs */

