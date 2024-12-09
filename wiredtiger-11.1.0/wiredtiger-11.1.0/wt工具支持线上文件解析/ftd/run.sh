rm -rf ./wtstat.data
ftdc-yyz-parse  decode  metrics.2024-05-11T11-15-29Z-00000   -o yang.log
#/root/yyz/ftdc-utils/cmd/ftdc/ftdc decode  metrics.2021-12-27T09-18-58Z-00000 --start="Mon Dec 27 17:50:49 CST 2021" --end="Mon Dec 27 18:05:49 CST 2021" -o yang.log
python2.7 wtstats.py wtstat.data
拷贝wtstats.html，用浏览器打开

