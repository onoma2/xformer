## TELEXo

The TELEXo (or TXo) is an output expander that adds an additional 4 Trigger and 4 CV jacks to the Teletype. There are jumpers on the back so you can hook more than one TXo to your Teletype simultaneously.

Outputs added to the system by the TELEX modules are addressed sequentially: 1-4 are on your first module of any type, 5-8 are on the second, 9-12 on the third, and so on. A few of the commands reference the module by its unit number – but those are rare.

Unlike the Teletype's equivalent operators, the TXo does not have get commands for its functions. This was intentional as these commands eat up processor and bus-space. While they may be added in the future, as of now you cannot poll the TXo for the current state of its various operators.

