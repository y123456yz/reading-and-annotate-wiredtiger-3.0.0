package ftdc

import (
	"fmt"
	"io"
	"os"
	"time"

	"gopkg.in/mgo.v2/bson"
)

// Chunk represents a 'metric chunk' of data in the FTDC
//readChunks中使用  全量和增量数据都记录到这里面
type Chunk struct {
	Metrics []Metric
	//总的增量数据点  协议中解析出来的，见 readChunks
	//后续获取到指定时间范围段的数据后，从新赋值为时间段范围的点数，参考 (c *Chunk) Clip
	NDeltas int
}

// Map converts the chunk to a map representation.
func (c *Chunk) Map() map[string]Metric {
	m := make(map[string]Metric)
	for _, metric := range c.Metrics {
		m[metric.Key] = metric
	}
	return m
}

// Clip trims the chunk to contain as little data as possible while keeping
// data within the given interval. If the chunk is entirely outside of the
// range, it is not modified and the return value is false.
func (c *Chunk) Clip(start, end time.Time) bool {
	st := start.Unix()
	et := end.Unix()
	var si, ei int
	for _, m := range c.Metrics {
		//解析出多个metrics中的start,5分钟一个间隔，也就是全量时候的时间点
		if m.Key != "start" {
			continue
		}

		//metrics的起始时间转换为秒   该metric起始时间，注意这个是全量时候的时间点
		mst := int64(m.Value) / 1000
		//加上增量部分监控，一般增量都是300秒，但是最后一个mtrics可能不到   也就是该metric最后一个增量数据的时间
		met := (int64(m.Value) + int64(sum(m.Deltas...))) / 1000
		if met < st || mst > et {//说明该metric没有想要的时间段数据
			return false // entire chunk outside range
		}

		//该metirc包含在时间段中
		if mst > st && met < et {
			return true // entire chunk inside range
		}

		//下面逻辑的数据都在时间段范围内

        //        fmt.Fprintf(os.Stderr, "yang test key time compare %v, %v 1111111111:%v\n", start, end, mst)
        //注意这个是全量时候的时间点
		t := mst

		for i := 0; i < c.NDeltas; i++ {
			t += int64(m.Deltas[i]) / 1000
			if t < st {
				si++
			}

			//
			if t < et {
				ei++
			} else {
				break
			}
		}
		if ei+1 < c.NDeltas {
			ei++ // inclusive of end time
		} else {
			ei = c.NDeltas - 1
		}
		break
	}

	//我们需要的时间段内的数据点总数，ei是结束时间对应的诊断数据，si是开始时间对应的诊断数据
	c.NDeltas = ei - si
	fmt.Fprintf(os.Stderr, "yang test key time NDeltas:%v\n", c.NDeltas)
	for _, m := range c.Metrics {
		m.Value += sum(m.Deltas[:si]...)
		m.Deltas = m.Deltas[si : ei+1]
	}
	return true
}

// Chunks takes an FTDC diagnostic file in the form of an io.Reader, and
// yields chunks on the given channel. The channel is closed when there are
// no more chunks.
//获取全量+变化增量的数据
func Chunks(r io.Reader, c chan<- Chunk) error {
	errCh := make(chan error)
	ch := make(chan bson.D)
	go func() {
		//读取bson解析后的metric内容写入ch管道
		errCh <- readDiagnostic(r, ch)
	}()
	go func() {
		//解析zlib压缩的数据，包括全量+增量传递给c
		errCh <- readChunks(ch, c)
	}()
	for i := 0; i < 2; i++ {
		err := <-errCh
		if err != nil {
			return err
		}
	}
	return nil
}

// Metric represents an item in a chunk.
//Chunk.Metrics成员为该类型
type Metric struct {
	// Key is the dot-delimited key of the metric. The key is either
	// 'start', 'end', or starts with 'serverStatus.'.
	//全量部分存key和value中
	Key string

	// Value is the value of the metric at the beginning of the sample
	Value int

	// Deltas is the slice of deltas, which accumulate on Value to yield the
	// specific sample's value.
	//增量部分存这里
	Deltas []int
	formattedTime string
	timestamp int64
}
