# Net::Remctl -- Perl bindings for the remctl client library.
# $Id$
#
# This is the Perl boostrap file for the Net::Remctl module, nearly all of
# which is implemented in XS.  For the actual source, see Remctl.xs.  This
# file contains the bootstrap and export code and the documentation.
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2007 Board of Trustees, Leland Stanford Jr. University
#
# See README for licensing terms.

package Net::Remctl;

use 5.006;
use strict;
use warnings;

# This should be kept at the same revision as the overall remctl package that
# it's part of for minimum confusion.
our $VERSION = '2.8';

require Exporter;
require DynaLoader;

our @ISA = qw(Exporter DynaLoader);
our @EXPORT = qw(remctl);

bootstrap Net::Remctl;
1;

=head1 NAME

Net::Remctl - Perl bindings for remctl (Kerberos remote command execution)

=head1 SYNOPSIS

    # Simplified form.
    use Net::Remctl;
    my $result = remctl("hostname", undef, undef, "test", "echo", "Hi");
    if ($result->error) {
        die "test echo failed with error ", $result->error, "\n";
    } else {
        warn $result->stderr;
        print $result->stdout;
        exit $result->status;
    }

    # Full interface.
    use Net::Remctl ();
    my $remctl = Net::Remctl->new;
    $remctl->open("hostname")
        or die "Cannot connect to hostname: ", $remctl->error, "\n";
    $remctl->command("test", "echo", "Hi there")
        or die "Cannot send command: ", $remctl->error, "\n";
    do {
        my $output = $remctl->output;
        if ($output->type eq 'output') {
            if ($output->stream == 1) {
                print $output->data;
            } elsif ($output->stream == 2) {
                warn $output->data;
            }
        } elsif ($output->type eq 'error') {
            warn $output->error, "\n";
        } elsif ($output->type eq 'status') {
            exit $output->status;
        } else {
            die "Unknown output token from library: ", $output->type, "\n";
        }
    } while ($output->type eq 'output');

=head1 DESCRIPTION

Net::Remctl provides Perl bindings to the libremctl client library.  remctl
is a protocol for remote command execution using GSS-API authentication.
The specific allowable commands must be listed in a configuration file on
the remote system and the remote system can map the remctl command names to
any local command without exposing that mapping to the client.  This module
implements a remctl client.

=head2 Simplified Interface

If you want to run a single command on a remote system and get back the
output and exit status, you can use the exported remctl() function:

=over 4

=item remctl(HOSTNAME, PORT, PRINCIPAL, COMMAND, [ARGS, ...])

Runs a command on the remote system and returns a Net::Remctl::Result object
(see below).  HOSTNAME is the remote host to contact.  PORT is the port of
the remote B<remctld> server and may be 0 or undef to tell the library to
use the default (4444).  PRINCIPAL is the principal of the server to use for
authentication; leave this blank to use the default of host/I<hostname> in
the default local realm.  The remaining arguments are the remctl command and
arguments passed to the remote server.

The return value is a Net::Remctl::Result object which supports the
following methods:

=over 4

=item error()

Returns the error message from either the remote host or from the local
client library (if, for instance, contacting the remote host failed).
Returns undef if there was no error.  Checking whether error() returns undef
is the supported way of determining whether the remctl() call succeeded.

=item stdout()

Returns the command's standard output or undef if there was none.

=item stderr()

Returns the command's standard error or undef if there was none.

=item status()

Returns the command's exit status.

=back

Each call to remctl() will open a new connection to the remote host and
close it after retrieving the results of the command.  To maintain a
persistant connection, use the full interface described below.

=back

=head1 Full Interface

The full remctl library interface requires that the user do more
bookkeeping, but it provides more flexibility and allows one to issue
multiple commands on the same persistent connection (provided that the
remote server supports protocol version two; if not, the library will
transparently fall back to opening a new connection for each command).

To use the full interface, first create a Net::Remctl object with new() and
then connect() to a remote server.  Then, issue a command() and call
output() to retrieve output tokens (as Net::Remctl::Output objects) until a
status token is received.  Destroying the Net::Remctl object will close the
connection.

The supported object methods are:

=over 4

=item new()

Create a new Net::Remctl object.  This doesn't attempt to connect to a host
and hence will only fail (by throwing an exception) if the library cannot
allocate memory.

=item error()

Retrieves the error message from the last failing operation and returns it
as a string.

=item connect(HOSTNAME[, PORT[, PRINCIPAL]])

Connect to HOSTNAME on port PORT using PRINCIPAL as the remote server's
principal for authentication.  If PORT is omitted, undef, or 0, use the
default port (4444).  If PRINCIPAL is omitted or undef, use the default of
host/I<hostname> in the local realm.  Returns true on success, false on
failure.  On failure, call error() to get the failure message.

=item command(COMMAND[, ARGS, ...])

Send the command and arguments to the remote host.  The command and the
arguments may, under the remctl protocol, contain any character, but be
aware that most remctl servers will reject commands or arguments containing
ASCII 0 (NUL), so currently this cannot be used for upload of arbitrary
unencoded binary data.  Returns true on success (meaning success in sending
the command, and implying nothing about the result of the command), false on
failure.  On failure, call error() to get the failure message.

=item output()

Returns the next output token from the remote host.  The token is returned
as a Net::Remctl::Output object, which supports the following methods:

=over 4

=item type()

Returns the type of the output token, which will be one of C<output>,
C<error>, C<status>, or C<done>.  A command will result in either one
C<error> token or zero or more C<output> tokens followed by a C<status>
token.  After either a C<error> or C<status> token is seen, another command
can be issued.  If the caller tries to retrieve another output token when it
has already consumed all of them for that command, the library will return a
C<done> token.

=item data()

Returns the contents of the token.  This method only makes sense for
C<output> and C<error> tokens; otherwise, it will return undef.  Note that
the returned value may contain any character, including ASCII 0 (NUL).

=item length()

Returns the length of the data in the token.  As with data(), this method
only makes sense for the C<output> and C<error> tokens.  It will return 0 if
there is no data or if the data is zero-length.

=item stream()

For an C<output> token, returns the stream with which the data is
associated.  Currently, only two stream values will be used: 1, meaning
standard output; and 2, meaning standard error.  The value is undefined for
all other output token types.

=item status()

For a C<status> token, returns the exit status of the remote command.  The
value is undefined for all other token types.

=item error()

For an C<error> token, returns the remctl error code for the protocol
error.  The text message will be returned by data().  The value is undefined
for all other token types.

=back

=back

Note that, due to internal implementation details in the library, the
Net::Remctl::Output object returned by output() will be invalidated by the
next call to command() or output() or by destroying the producing
Net::Remctl object.  Therefore, any data in the output token should be
processed and stored if needed before making any further Net::Remctl method
calls on the same object.

=head1 SEE ALSO

remctl(1), remctld(8)

The current version of this module is available from its web page at
L<http://www.eyrie.org/~eagle/software/remctl/>.

=head1 AUTHOR

Russ Allbery <rra@stanford.edu>

=head1 COPYRIGHT AND LICENSE

Copyright 2007 Board of Trustees, Leland Stanford Jr. University.  All
rights reserved.

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted, provided
that the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation, and that the name of Stanford University not be used in
advertising or publicity pertaining to distribution of the software without
specific, written prior permission.  Stanford University makes no
representations about the suitability of this software for any purpose.  It
is provided "as is" without express or implied warranty.

THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

=cut