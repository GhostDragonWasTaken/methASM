#ifndef LINK_UNRESOLVED_HINT_H
#define LINK_UNRESOLVED_HINT_H

#include "linker/symbol_resolve.h"

void link_unresolved_format(const LinkResolution *resolution,
                            const char *symbol_name,
                            char **error_message_out);

#endif
