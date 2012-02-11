/*
 * Utility functions for tests that use remctl.
 *
 * Provides functions to start and stop a remctl daemon that uses the test
 * Kerberos environment and runs on port 14373 instead of the default 4373.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2006, 2007, 2009, 2011, 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef TAP_REMCTL_H
#define TAP_REMCTL_H 1

#include <config.h>
#include <portable/macros.h>

#include <sys/types.h>          /* pid_t */

/* Defined in <tests/tap/kerberos.h>. */
struct kerberos_config;

BEGIN_DECLS

/*
 * Start and stop remctld for tests that use it.  kerberos_setup should
 * normally be called first to check whether a Kerberos configuration is
 * available and to set KRB5_KTNAME.  Takes the path to remctld, which may be
 * found via configure, the Kerberos configuration, the path to the
 * configuration file, and then any additional arguments to remctld,
 * terminated by NULL.
 *
 * remctl_stop can be called explicitly to stop remctld and clean up, but it's
 * also registered as an atexit handler, so tests that only start and stop the
 * server once can just let cleanup happen automatically.
 */
pid_t remctld_start(const char *path, struct kerberos_config *,
                    const char *config, ...)
    __attribute__((__nonnull__(1, 2, 3)));
void remctld_stop(void);

END_DECLS

#endif /* !TAP_REMCTL_H */
