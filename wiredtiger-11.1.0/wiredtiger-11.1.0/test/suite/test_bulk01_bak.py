#!/usr/bin/env python
#
# Public Domain 2014-present MongoDB, Inc.
# Public Domain 2008-2014 WiredTiger, Inc.
#
# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
# test_bulk.py
#       bulk-cursor test.
#

import wiredtiger, wttest
from wtdataset import simple_key, simple_value
from wtscenario import make_scenarios

# Smoke test bulk-load.
class test_bulk_load(wttest.WiredTigerTestCase):
    name = 'test_bulk'

    types = [
        ('file', dict(type='file:')),
        ('table', dict(type='table:'))
    ]
    keyfmt = [
        ('integer', dict(keyfmt='i')),
        ('recno', dict(keyfmt='r')),
        ('string', dict(keyfmt='S')),
    ]
    valfmt = [
        ('fixed', dict(valfmt='8t')),
        ('integer', dict(valfmt='i')),
        ('string', dict(valfmt='S')),
    ]
    scenarios = make_scenarios(types, keyfmt, valfmt)

    # Test a simple bulk-load
    def test_bulk_load(self):
        uri = self.type + self.name
        self.session.create(uri,
            'key_format=' + self.keyfmt + ',value_format=' + self.valfmt)
        cursor = self.session.open_cursor(uri, None, "bulk")
        for i in range(1, 1000):
            cursor[simple_key(cursor, i)] = simple_value(cursor, i)

if __name__ == '__main__':
    wttest.run()
