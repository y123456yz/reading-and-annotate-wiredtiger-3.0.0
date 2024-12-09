package ftdc

import (
	"bufio"
	"bytes"
	"compress/zlib"
	"encoding/binary"
	"fmt"
	"gopkg.in/mgo.v2/bson"
	"io"
        "strings"   
//        "os"
//        "time"
)

//从metric文件中读取数据写入ch
func readDiagnostic(f io.Reader, ch chan<- bson.D) error {
	buf := bufio.NewReader(f)
	for {
		//从buf对应文件读取bson解析后的数据到doc中
		doc, err := readBufBSON(buf)
		if err != nil {
			if err == io.EOF {
				break
			}
			return err
		}
		ch <- doc
	}
	close(ch)
	return nil
}

func readChunks(ch <-chan bson.D, o chan<- Chunk) error {
	//ch中存储着metric bson解析后的数据，配合readDiagnostic阅读
	for doc := range ch {//5分钟一个全量+5分钟内每秒一个增量
		//也就是bson解析完成后的数据
		m := doc.Map()
		if m["type"] == 1 {
			//获取data部分的数据内容
			zBytes := m["data"].([]byte)[4:]
			//zlib解压
			z, err := zlib.NewReader(bytes.NewBuffer(zBytes))
			if err != nil {
				fmt.Print(err)
				return err
			}
			buf := bufio.NewReader(z)
			//全量部分数据解析存入metrics.key和Metric.value中
			metrics, err := readBufMetrics(buf)
			if err != nil {
				fmt.Print(err)
				return err
			}
			bl := make([]byte, 8)
			//增量部分数据长度
			_, err = io.ReadAtLeast(buf, bl, 8)
			if err != nil {
				fmt.Print(err)
				return err
			}
			//nmetrics := unpackInt(bl[:4])
			//ndeltas := unpackInt(bl[4:])
			nmetrics := int(binary.LittleEndian.Uint32(bl[:4]))
			ndeltas := int(binary.LittleEndian.Uint32(bl[4:]))
                        if nmetrics != len(metrics) {
				return fmt.Errorf("metrics mismatch. Expected %d, got %d", nmetrics, len(metrics))
			}
			nzeroes := 0
			//num := 0


			for i, v := range metrics {
				dealKey := v.Key
				dealKey = strings.TrimPrefix(dealKey, "serverStatus.wiredTiger.")
				//这里打印的是全量数据
				//fmt.Fprintf(os.Stderr, "yang test key metrics[i].key:%v, v.Value:%v\n", dealKey, v.Value)
				metrics[i].Value = v.Value
				metrics[i].Deltas = make([]int, ndeltas)

				for j := 0; j < ndeltas; j++ {
					var delta int
					if nzeroes != 0 {
						delta = 0
						nzeroes--
					} else {
						delta, err = unpackDelta(buf)
						if err != nil {
							return err
						}
						if delta == 0 {
							nzeroes, err = unpackDelta(buf)
							if err != nil {
								return err
							}
						}
					}
					//这里是增量部分，也就是差值
					//fmt.Fprintf(os.Stderr, "yang test key inc  key:%s, value:%d\n", dealKey, delta)
					metrics[i].Deltas[j] = delta
					//num = j
				}
				//fmt.Fprintf(os.Stderr, "yang test key metrics[i].Deltas count:%v\n", num)
			}


			//通过管道传递
			o <- Chunk{
				Metrics: metrics,
				NDeltas: ndeltas,
			}
		}
	}
	close(o)
	return nil
}

//读取metric文件存入d
func readBufDoc(buf *bufio.Reader, d interface{}) (err error) {
	var bl []byte
	bl, err = buf.Peek(4)
	if err != nil {
		return
	}
	l := unpackInt(bl)

	b := make([]byte, l)
	_, err = io.ReadAtLeast(buf, b, l)
	if err != nil {
		return
	}
	err = bson.Unmarshal(b, d)
	return
}

//读取metric文件存入doc
func readBufBSON(buf *bufio.Reader) (doc bson.D, err error) {
	err = readBufDoc(buf, &doc)
	return
}

//全量部分数据解析存入metrics
func readBufMetrics(buf *bufio.Reader) (metrics []Metric, err error) {
	doc := bson.D{}
	err = readBufDoc(buf, &doc)
	if err != nil {
		return
	}
	//全量部分数据解析
	metrics = flattenBSON(doc)
	return
}
