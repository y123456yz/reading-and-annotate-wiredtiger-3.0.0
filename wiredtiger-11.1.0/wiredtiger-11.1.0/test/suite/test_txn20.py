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
# test_txn20.py
#   Transactions: more granular testing of isolation levels
#

import wttest
from wtscenario import make_scenarios

class test_txn20(wttest.WiredTigerTestCase):
    uri = 'table:test_txn'

    format_values = [
        ('string-row', dict(key_format='S', key='key', \
                            value_format='S', old_value='value:old', new_value='value:new')),
        ('column', dict(key_format='r', key=12, \
                            value_format='S', old_value='value:old', new_value='value:new')),
        ('column-fix', dict(key_format='r', key=12, \
                            value_format='8t', old_value=89, new_value=167)),
    ]
    iso_types = [
        ('isolation_read_uncommitted', dict(isolation='read-uncommitted')),
        ('isolation_read_committed', dict(isolation='read-committed')),
        ('isolation_snapshot', dict(isolation='snapshot'))
    ]
    scenarios = make_scenarios(format_values, iso_types)
    old_value = 'value: old'
    new_value = 'value: new'

    def test_isolation_level(self):
        #self.conn.reconfigure("verbose=[]") #yang add change
        # 没有显示定义begin_transaction， 则在cursor[self.key] = self.old_value写数据的时候会自动隐式的begin和commit
        config = 'key_format={},value_format={}'.format(self.key_format, self.value_format)
        self.session.create(self.uri, config)
        cursor = self.session.open_cursor(self.uri, None)
        cursor[self.key] = self.old_value

        # Make an update and don't commit it just yet. We should see the update
        # from the 'read-uncommitted' isolation level.
        # session显示的创建事务，并写入数据，但没有提交事务
        self.session.begin_transaction()
        cursor[self.key] = self.new_value
        
        # 新session s在这里创建新的session，然后begin事务读取数据
        s = self.conn.open_session()
        # 新session s对应的cursor
        cursor = s.open_cursor(self.uri, None)
        s.begin_transaction('isolation=' + self.isolation)

        if self.isolation == 'read-uncommitted':
            # Unlike the 'read-committed' and 'snapshot' isolation levels, we're
            # not protected from dirty reads so we'll see the update above even
            # though its respective transaction has not been committed.
            # cursor[self.key]实际上就是通过cursor从新获取self.key的值
            self.assertEqual(cursor[self.key], self.new_value)
        else:
            self.assertEqual(cursor[self.key], self.old_value)

        # Commit the update now. We should see the update from the
        # 'read-committed' and 'read-uncommitted' isolation levels.
        self.session.commit_transaction()
        
        # 由于s在读取之前进行了begin_transaction，这个时候s已经获取到了前面session new_value的事务快照存到snapshot[]数组中，由于在sesison new_value在snapshot[]中，因此__txn_visible_id返回无法读取
        
        # 因此snapshot和read-committed区别是，snapshot以begin_transaction事务开始时当时的快照snapshot[]为准，而read-committed以读取数据时候的快照snapshot[]为准，
        #   中间的gap是这期间如果有事务已提交,则snapshot[]中就不会有已提交的事务，最终影响__txn_visible_id的访问性
        if self.isolation == 'snapshot':
            # We should never see the updates above since it wasn't committed at
            # the time of the snapshot.
            self.assertEqual(cursor[self.key], self.old_value)
        else:
            # Unlike the 'snapshot' isolation level, 'read-committed' is not
            # protected from non-repeatable reads so we'll see an update
            # that wasn't visible earlier in our previous read. As before,
            # 'read-uncommitted' will still see the new value.
            self.assertEqual(cursor[self.key], self.new_value)

if __name__ == '__main__':
    wttest.run()
