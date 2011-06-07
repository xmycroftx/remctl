package org.eyrie.remctl.core;

import java.io.DataOutputStream;
import java.io.IOException;

import org.eyrie.remctl.RemctlException;
import org.ietf.jgss.GSSContext;

/**
 * Represents a remctl status token. This is sent from the server to the client
 * and represents the end of output from a command. It carries the status code
 * of the command, which is zero for success and non-zero for some sort of
 * failure.
 */
public class RemctlStatusToken extends RemctlMessageToken {
    /** Status (exit) code of command. */
    private final int status;

    /**
     * Construct a status token with the given status code.
     * 
     * @param context
     *            GSS-API context used for encryption
     * @param status
     *            The exit status code of the command, which must be between 0
     *            and 255, inclusive.
     * @throws RemctlException
     *             Thrown if the status parameter is out of range.
     */
    RemctlStatusToken(GSSContext context, int status)
            throws RemctlException {
        super(context, 2, RemctlMessageCode.MESSAGE_STATUS);
        if (status < 0 || status > 255) {
            throw new RemctlException("status " + status + " out of range");
        }
        this.status = status;
    }

    RemctlStatusToken(GSSContext context, byte[] data)
            throws RemctlException {
        super(context, 2, RemctlMessageCode.MESSAGE_OUTPUT);

        //FIXME: validate data length
        this.status = data[0];

    }

    /**
     * Determine the length of this status message token.
     * 
     * @return The length of the wire representation of the data payload of this
     *         status token
     */
    @Override
    int length() {
        return 1;
    }

    /**
     * Write the wire representation of this status token to the provided output
     * stream.
     * 
     * @param stream
     *            Output stream to which to write the encoded token
     * @throws IOException
     *             On errors writing to the output stream
     */
    @Override
    void writeData(DataOutputStream stream) throws IOException {
        stream.writeByte(this.status);
    }

    /**
     * @return the status
     */
    int getStatus() {
        return this.status;
    }

    /**
     * Check if the status was success
     * 
     * @return true if status was 0, or false for non-zero return codes.
     */
    boolean isSuccessful() {
        return 0 == this.status;
    }

    /* (non-Javadoc)
     * @see java.lang.Object#toString()
     */
    @Override
    public String toString() {
        return "RemctlStatusToken [status=" + this.status + "]";
    }
}