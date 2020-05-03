#!/bin/bash

DAEMON=/path/to/bcexchanged
ARGS="-testnet"
VOTE_PATH=/path/to/output/vote.json
SIGNATURE_PATH=/path/to/output/vote.json.signature
ADDRESS=S....

$DAEMON $ARGS getvote >$VOTE_PATH
$DAEMON $ARGS signmessage $ADDRESS <$VOTE_PATH >$SIGNATURE_PATH
