wt verbose生效需要server启用下面这个配置，改mongod版本wiredtiger_open的verbose=[]中我把所有的模块都加进去了
db.setLogLevel(5, 'storage');

