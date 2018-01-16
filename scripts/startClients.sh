#!/usr/bin/env bash

####################################################################
# The following license applies to the script in this file:
#
####################################################################
#
# Boost Software License - Version 1.0 - August 17th, 2003
#
# Permission is hereby granted, free of charge, to any person or organization
# obtaining a copy of the software and accompanying documentation covered by
# this license (the "Software") to use, reproduce, display, distribute,
# execute, and transmit the Software, and to prepare derivative works of the
# Software, and to permit third-parties to whom the Software is furnished to
# do so, all subject to the following:
#
# The copyright notices in the Software and this entire statement, including
# the above license grant, this restriction and the following disclaimer,
# must be included in all copies of the Software, in whole or in part, and
# all derivative works of the Software, unless such copies or derivative
# works are solely in the form of machine-executable object code generated by
# a source language processor.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
# SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
# FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#
####################################################################
#
# Author: Dr. Rüdiger Berlich of Gemfony scientific UG (haftungsbeschraenkt)
# See http://www.gemfony.eu for further information.
#
####################################################################
# This script will start a given "Evaluator" program in client mode a
# predefined number of times. The server needs to be started manually.
####################################################################

# Check the number of command line arguments (should be exactly 4)
if [ ! $# -eq 4 ]; then
    echo "Usage: ./startClients.sh <program name> <number of clients> <ip/hostname> <port>"
    exit 1
fi

# Read in the command line arguments
PROGNAME=$1
NCLIENTS=$2
IP=$3
PORT=$4

# Check that the program exists
if [ ! -e ${PROGNAME} ]; then
    echo "Error: Program file ${PROGNAME} does not exist."
    exit
fi

# Check that the desired number of clients is integral and >= 0
if [ ! $(echo "${NCLIENTS}" | grep -E "^[0-9]+$") ]; then
    echo "Error: Number of clients Number of clients \"${NCLIENTS}\" is not a valid integer. Leaving."
    exit
fi
if [ ! ${NCLIENTS} -gt 0 ];     then
    echo "Error: \"${NCLIENTS}\" should at least be 1. Leaving"
    exit
fi

# Check that the port number is integral and >= 1000
if [ ! $(echo "${PORT}" | grep -E "^[0-9]+$") ]; then
    echo "Error: Port \"${PORT}\" is not a valid integer. Leaving."
    exit
fi
if [ ${PORT} -le 1000 ];     then
    echo "Error: Port \"${PORT}\" should at least be 1001. Leaving"
    exit
fi

# Create an output directory
if [ ! -d ./output ]; then
    mkdir ./output
fi

# Start the clients
for i in `seq 1 $2`; do
    (./${PROGNAME} --client --host=${IP} --port=${PORT} >& ./output/output_client_$i) &
done
