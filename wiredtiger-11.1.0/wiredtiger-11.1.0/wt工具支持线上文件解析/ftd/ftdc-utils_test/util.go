package ftdc

import (
	"bufio"
	"bytes"
	"encoding/binary"
//	"fmt"
//	"os"
//	"reflect"
	"time"

	"gopkg.in/mgo.v2/bson"
	//	"github.com/evergreen-ci/birch/bsontype"
)

func flattenBSONTop(d bson.D) (metrics []Metric) {
	for _, e := range d {
		if e.Name == "wiredTiger" {
			metrics = flattenBSON(d)
			return metrics
		}
	}
	return nil
}

func flattenBSON(d bson.D) (o []Metric) {
	for _, e := range d {
                //fmt.Fprintf(os.Stderr, "yang test .....flattenBSON.....key:%v\n", e.Name)
		switch child := e.Value.(type) {
		case bson.D:
			n := flattenBSON(child)
			for _, ne := range n {
				o = append(o, Metric{
					Key:   e.Name + "." + ne.Key,
					Value: ne.Value,
				})
			}
		case []interface{}: // skip
		
			iter_arry := e.Value.([]interface{}) // ignore the error which can never be non-nil
			for _, i := range iter_arry {
				tmp_i, tmp_j := i.(bson.D)
				if tmp_j == false {
					//fmt.Fprintf(os.Stderr, "yang test key []interface{}:%v\n", e.Value)
					continue
				}

				n := flattenBSON(tmp_i)
				for _, ne := range n {
					o = append(o, Metric{
						Key:   e.Name + "." + ne.Key,
						Value: ne.Value,
					})
				}
			}
                case string: // skip
                   // fmt.Fprintf(os.Stderr, "yang test string:%v\n", e)
		case bool:
			if child {
				o = append(o, Metric{
					Key:   e.Name,
					Value: 1,
				})
			} else {
				o = append(o, Metric{
					Key:   e.Name,
					Value: 0,
				})
			}
		case float64:
			o = append(o, Metric{
				Key:   e.Name,
				Value: int(child),
			})
		case int:
			o = append(o, Metric{
				Key:   e.Name,
				Value: child,
			})
		case int32:
			o = append(o, Metric{
				Key:   e.Name,
				Value: int(child),
			})
		case int64:
			o = append(o, Metric{
				Key:   e.Name,
				Value: int(child),
			})
//		case time.Time:
//			o = append(o, Metric{
//				Key:   e.Name,
//				Value: int(child.Unix()) * 1000,
//			})
//		default:
//			return []Metric{}

		case time.Time:
			//t, i := e.Value.Timestamp()
  //                  fmt.Fprintf(os.Stderr, "yang test time key:%v\n", reflect.TypeOf(e.Value))
			o = append(o, Metric{
				Key:   e.Name,
				Value: int(child.Unix()) * 1000,
			})
//			o = append(o, Metric{
//				Key:   e.Name+ ".inc",
//				//Value: int(child.Unix()) * 1000,
//				Value:int(child.Unix()),
//			})





		case bson.MongoTimestamp:
                        v := e.Value.(bson.MongoTimestamp)
                       // val := binary.LittleEndian.Uint32(v.data[v.offset+4 : v.offset+8]), binary.LittleEndian.Uint32(v.data[v.offset : v.offset+4])
                        v2 := v / (1 << 32)
    //                    fmt.Println(time.Unix(int64(v2), 0).Format("2006-01-02 15:04:05"), v2)
			o = append(o, Metric{
				Key:   e.Name,
				Value: int(v2) * 1000,
			})
			o = append(o, Metric{
				Key:   e.Name+ ".inc",
				Value: 0,
			})






		default:
                        //fmt.Fprintf(os.Stderr, "yang test key default:%v\n", reflect.TypeOf(e.Value))
//			o = append(o, Metric{
//				Key:   e.Name,
//				Value: 0,
//			})
		}
	}
	return o
}

func unpackDelta(buf *bufio.Reader) (delta int, err error) {
	var res uint64
	var shift uint
	for {
		var b byte
		b, err = buf.ReadByte()
		if err != nil {
			return
		}
		bb := uint64(b)
		res |= (bb & 0x7F) << shift
		if bb&0x80 == 0 {
			// read as int64 (handle negatives)
			var n int64
			tmp := make([]byte, 8)
			binary.LittleEndian.PutUint64(tmp, res)
			binary.Read(bytes.NewBuffer(tmp), binary.LittleEndian, &n)
			delta = int(n)
			return
		}
		shift += 7
	}
}

func unpackInt(bl []byte) int {
	return int(int32((uint32(bl[0]) << 0) |
		(uint32(bl[1]) << 8) |
		(uint32(bl[2]) << 16) |
		(uint32(bl[3]) << 24)))
}

func sum(l ...int) (s int) {
	for _, v := range l {
		s += v
	}
	return
}

func square(n int) int {
	return n * n
}
