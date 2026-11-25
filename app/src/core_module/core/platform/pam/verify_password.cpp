#include "verify_password.h"

#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>

namespace orion::pam {

static int
conv(int num_msg, const struct pam_message** msg, struct pam_response** resp, void* appdata_ptr)
{
    if (num_msg != 1) return PAM_CONV_ERR;

    // We must allocate the pam_response array. PAM will free the memory when it is no longer needed.
    struct pam_response* response = static_cast<pam_response*>(calloc(1, sizeof(struct pam_response)));
    if (!response) return PAM_CONV_ERR;

    // We must allocate the resp string. PAM will free the memory when it is no longer needed.
    response[0].resp = strdup(static_cast<const char*>(appdata_ptr)); // password
    response[0].resp_retcode = 0;

    *resp = response;
    return PAM_SUCCESS;
}

bool
verify_password(const char* password)
{
    const char* user = getpwuid(getuid())->pw_name;

    struct pam_conv pam_conversation = { conv, (void*)password };
    pam_handle_t* pamh = NULL;

    // Note: PAM must be configured to allow "login" authentication (default on almost all Linux systems)
    int status = pam_start("login", user, &pam_conversation, &pamh);
    if (status != PAM_SUCCESS) return 0;

    status = pam_authenticate(pamh, 0);
    pam_end(pamh, status);

    return status == PAM_SUCCESS;
}

} // namespace orion::pam