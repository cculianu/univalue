// Test program that can be called by the JSON test suite at
// https://github.com/nst/JSONTestSuite.
//
// It reads JSON input from stdin and exits with code 0 if it can be parsed
// successfully. It also pretty prints the parsed JSON value to stdout.

#include <iostream>
#include <string>
#include "univalue.h"

using namespace std;

int main()
{
    UniValue val;
    if (val.read(string(istreambuf_iterator<char>(cin),
                        istreambuf_iterator<char>()))) {
        cout << UniValue::stringify(val, 1 /* prettyIndent */) << endl;
        return 0;
    } else {
        cerr << "JSON Parse Error." << endl;
        return 1;
    }
}
