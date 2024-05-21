rm -rf ./wtstat.data
ftdc-yyz-parse  decode  metrics.2024-05-11T11-15-29Z-00000   -o yang.log
python2.7 wtstats.py wtstat.data
拷贝wtstats.html，用浏览器打开

