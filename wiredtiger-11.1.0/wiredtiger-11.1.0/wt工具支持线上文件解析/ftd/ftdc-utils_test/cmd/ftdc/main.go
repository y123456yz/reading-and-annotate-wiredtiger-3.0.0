package main

import (
	"encoding/json"
	"fmt"
	"math"
	"os"
	"sort"
	"time"
        "strings"
	"github.com/vmenajr/ftdc-utils"
	"github.com/jessevdk/go-flags"
)

func main() {
	opts := struct{}{}
	parser := flags.NewParser(&opts, flags.Default)
	parser.AddCommand("decode", "decode diagnostic files into raw JSON output", "", &DecodeCommand{})
	parser.AddCommand("stats", "read diagnostic file(s) into aggregated statistical output", "", &StatsCommand{})
	parser.AddCommand("compare", "compare statistical output", "", &CompareCommand{})

	_, err := parser.Parse()
	if err != nil {
		os.Exit(1)
	}
}

type DecodeCommand struct {
	StartTime string `long:"start" value-name:"<TIME>" description:"clip data preceding start time (layout UnixDate)"`
	EndTime   string `long:"end" value-name:"<TIME>" description:"clip data after end time (layout UnixDate)"`
	Merge     bool   `short:"m" long:"merge" description:"merge chunks into one object"`
	Out       string `short:"o" long:"out" value-name:"<FILE>" description:"write diagnostic output, in JSON, to given file" required:"true"`
	Silent    bool   `short:"s" long:"silent" description:"suppress chunk overview output"`
	Args      struct {
		Files []string `positional-arg-name:"FILE" description:"diagnostic file(s)"`
	} `positional-args:"yes" required:"yes"`
}

func (decOpts *DecodeCommand) Execute(args []string) error {
	if len(args) > 0 {
		return fmt.Errorf("unknown argument: %s", args[0])
	}

	output, err := decode(decOpts.Args.Files, decOpts.StartTime, decOpts.EndTime, decOpts.Silent, decOpts.Merge)
	if err != nil {
		return err
	}
	err = writeJSONtoFile(output, decOpts.Out)
	return err
}

type StatsCommand struct {
	StartTime string `long:"start" value-name:"<TIME>" description:"clip data preceding start time (layout UnixDate)"`
	EndTime   string `long:"end" value-name:"<TIME>" description:"clip data after end time (layout UnixDate)"`
	Out       string `short:"o" long:"out" value-name:"<FILE>" description:"write stats output, in JSON, to given file" required:"true"`
	Args      struct {
		Files []string `positional-arg-name:"FILE" description:"diagnostic file(s)"`
	} `positional-args:"yes" required:"yes"`
}

func (statOpts *StatsCommand) Execute(args []string) error {
	if len(args) > 0 {
		return fmt.Errorf("unknown argument: %s", args[0])
	}
	output, err := stats(statOpts.Args.Files, statOpts.StartTime, statOpts.EndTime)
	if err != nil {
		return err
	}
	err = writeJSONtoFile(output, statOpts.Out)
	return err
}

type CompareCommand struct {
	Explicit  bool    `short:"e" long:"explicit" description:"show comparison values for all compared metrics; sorted by score, descending"`
	Threshold float64 `short:"t" long:"threshold" value-name:"<FLOAT>" description:"threshold of deviation in comparison" default:"0.2"`
	Args      struct {
		FileA string `positional-arg-name:"STAT1" description:"statistical file (JSON)"`
		FileB string `positional-arg-name:"STAT2" description:"statistical file (JSON)"`
	} `positional-args:"yes" required:"yes"`
}

func (cmp *CompareCommand) Execute(args []string) error {
	if len(args) > 0 {
		return fmt.Errorf("unknown argument: %s", args[0])
	}
	ftdc.CmpThreshold = cmp.Threshold
	sa, err := readJSONStats(cmp.Args.FileA)
	if err != nil {
		return err
	}
	sb, err := readJSONStats(cmp.Args.FileB)
	if err != nil {
		return err
	}

	score, scores, ok := ftdc.Proximal(sa, sb)
	// score to stdout, scores to stdout, ok to status code
	sort.Sort(sort.Reverse(scores))
	var msg string
	for _, s := range scores {
		if cmp.Explicit {
			fmt.Printf("%5f: %s\n", s.Score, s.Metric)
		}
		if s.Err != nil {
			msg += s.Err.Error()
		}
	}
	fmt.Fprintln(os.Stderr, msg)
	fmt.Printf("score: %f\n", score)
	var result string
	if ok {
		result = "SUCCESS"
	} else {
		result = "FAILURE"
	}

	err = fmt.Errorf("comparison completed. result: %s", result)
	if ok {
		fmt.Fprintln(os.Stderr, err)
		return nil
	}
	return err
}

func readJSONStats(file string) (s ftdc.Stats, err error) {
	f, err := os.Open(file)
	if err != nil {
		return
	}
	err = json.NewDecoder(f).Decode(&s)
	f.Close()
	return
}

func parseTimes(tStart, tEnd string) (start, end time.Time, err error) {
	if tStart != "" {
		start, err = time.Parse(time.UnixDate, tStart)
		if err != nil {
			err = fmt.Errorf("error: failed to parse start time '%s': %s", tStart, err)
			return
		}
	} else {
		start = time.Unix(math.MinInt64, 0)
	}
	if tEnd != "" {
		end, err = time.Parse(time.UnixDate, tEnd)
		if err != nil {
			err = fmt.Errorf("error: failed to parse end time '%s': %s", tEnd, err)
			return
		}
	} else {
		end = time.Unix(math.MaxInt64, 0)
	}
	return
}

func stats(files []string, tStart, tEnd string) (interface{}, error) {
	if len(files) == 0 {
		return nil, fmt.Errorf("error: must provide FILE")
	}

	start, end, err := parseTimes(tStart, tEnd)
	if err != nil {
		return nil, err
	}

	ss := []ftdc.Stats{}
	for _, file := range files {
		f, err := os.Open(file)
		if err != nil {
			return nil, fmt.Errorf("error: failed to open '%s': %s", file, err)
		}

		cs, err := ftdc.ComputeStatsInterval(f, start, end)
		if err != nil {
			return nil, err
		}
		ss = append(ss, cs...)
		f.Close()
	}

	if len(ss) == 0 {
		return nil, fmt.Errorf("no chunks found")
	}
	ms := ftdc.MergeStats(ss...)
	fmt.Fprintf(os.Stderr, "yang yazhou result:found %d samples\n", ms.NSamples)

	return ftdc.MergeStats(ss...), nil
}

var metricsTmp[2000] int
func decode(files []string, tStart, tEnd string, silent, shouldMerge bool) (interface{}, error) {
	if len(files) == 0 {
		return nil, fmt.Errorf("error: must provide FILE")
	}

	start, end, err := parseTimes(tStart, tEnd)
	if err != nil {
		return nil, err
	}

	// this will consume a LOT of memory
	cs := []ftdc.Chunk{}
	count := 0
	for _, file := range files {
		f, err := os.Open(file)
		if err != nil {
			return nil, fmt.Errorf("error: failed to open '%s': %s", file, err)
		}
                fmt.Fprintf(os.Stderr, "yang test 11111111111111111111111111111111 decode\n")

		o := make(chan ftdc.Chunk)
		go func() {
			err := ftdc.Chunks(f, o)
			if err != nil {
				fmt.Fprintf(os.Stderr, "error: failed to parse chunks: %s\n", err)
			}
		}()

		logChunk := func(c ftdc.Chunk) {
			t := time.Unix(int64(c.Map()["start"].Value)/1000, 0).Format(time.UnixDate)
		        fmt.Fprintf(os.Stderr, "yang yazhou result:chunk of file '%s' with %d stastic and "+
				"%d elems on %s\n", file, len(c.Metrics), c.NDeltas, t)
                }


		os.Remove("./wtstat.data")
		f_w, err := os.OpenFile("./wtstat.data", os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
		if err != nil {
			return nil, err
		}
		defer f_w.Close()
                o_len := 0
		for c := range o {
			if !c.Clip(start, end) {
				continue
			}
			if !silent {
				logChunk(c)
			}
                        o_len++

			cs = append(cs, c)
			count += c.NDeltas

			timestamp := int64(c.Map()["start"].Value) / 1000
                        now := time.Unix(timestamp, 0)
			timeStr := now.Format("Jan 02 15:04:05")
                        //Deltas[]内容可能为负数，所以这里最好用无符号
                        var totalValue int = 0

			//存每个统计项第一次开始的value值，因为有些统计项不是递增的，
			for j := 0; j < len(c.Metrics[0].Deltas); j++ {
				for i := 0; i < len(c.Metrics); i++ {
					Deltas := c.Metrics[i]
					key := Deltas.Key
					if !strings.Contains(key, "serverStatus.wiredTiger.") {
						continue
					}

					if strings.Contains(key, "serverStatus.wiredTiger.concurrentTransactions") ||
						strings.Contains(key, "serverStatus.wiredTiger.snapshot-window-settings") ||
						strings.Contains(key, "serverStatus.wiredTiger.oplog") ||
							strings.Contains(key, "serverStatus.wiredTiger.thread-state"){
						continue
					}

				        //if len(c.Metrics[0].Deltas) < 200 {
					//    fmt.Printf("Metrics Deltas:%d < 200, skip", c.Metrics[0].Deltas)
					//    continue
				        //}
					dealKey := key
					dealKey = strings.TrimPrefix(dealKey, "serverStatus.wiredTiger.")
					dealKey = strings.Replace(dealKey, ".", ": ", 1)
					//value := c.Metrics[i].Deltas[j]

					continuouslyIncreasing := true
                                        realTime := true
					if strings.Contains(key, "bytes belonging to") || strings.Contains(key, "transaction checkpoint generation") ||
                                                strings.Contains(key, "bytes not belonging to page images in the cache") ||
						strings.Contains(key, "eviction state") || strings.Contains(key, "files with active eviction walks") ||
						strings.Contains(key, "history store table updates without timestamps fixed up by reinserting with the fixed timestamp")  ||
						strings.Contains(key, "hazard pointer maximum array length") || strings.Contains(key, "history store table on-disk size") ||
						strings.Contains(key, "tracked dirty bytes in the cache")  ||
						strings.Contains(key, "maximum page size at eviction") || strings.Contains(key, "pages currently held in the cache") ||
						strings.Contains(key, "tracked")  ||
						strings.Contains(key, "cached cursor count") || strings.Contains(key, "open cursor count") ||
						strings.Contains(key, "connection data handles currently active")  ||
						strings.Contains(key, "log sync time duration (usecs)") || strings.Contains(key, "log sync_dir time duration (usecs)") ||
						strings.Contains(key, "slot joins yield time (usecs)")  ||
						strings.Contains(key, "bytes belonging to page images in the cache") ||
						strings.Contains(key, "maximum seconds spent in a reconciliation call") || strings.Contains(key, "split bytes currently awaiting free") ||
					        strings.Contains(key, "transaction checkpoint currently running")  ||
                                                strings.Contains(key, "bytes currently in the cache")  ||
						strings.Contains(key, "transaction range of ")  ||
                                                strings.Contains(key, "split objects currently awaiting free")  {
						continuouslyIncreasing = false
                                                realTime = true
					}
                                       
					//if strings.Contains(key, "bytes allocated for updates")  {
					//	continuouslyIncreasing = false
					//	realTime = true
					//}
 
					if o_len == 1 && j == 0 {
						metricsTmp[i] = c.Metrics[i].Value
						if strings.Contains(key, "bytes dirty in the cache cumulative") {
							//fmt.Printf("yang test .....1.......... metricsTmp i:%d, metricsTmp:%d\n", i, metricsTmp[i])
						}
					}
					if continuouslyIncreasing == false {
						totalValue = Deltas.Value
						if realTime == false {
							totalValue -= metricsTmp[i]
						}
                                                if o_len == 1 && j == 0 && strings.Contains(key, "bytes dirty in the cache cumulative") {
                                                      //  fmt.Printf("yang test ......2......... metricsTmp i:%d, totalValue:%d\n", i, totalValue)
                                                }
					} else {
						totalValue = 0
					}

					for k := 0; k < j; k++ {
						totalValue += c.Metrics[i].Deltas[k]
					}

                                                if o_len == 1 && j == 0 && strings.Contains(key, "bytes allocated for updates") {
                                                        //fmt.Printf("yang test ......3......... metricsTmp i:%d, totalValue:%d\n", i, totalValue)
                                                }
					logLine := timeStr +" " + fmt.Sprintf("%d", totalValue) + " ./ " + dealKey + "\n"
					//fmt.Printf("%s %d ./ %s\n", timeStr, value, dealKey)
					//fmt.Printf("%s", logLine)
					f_w.WriteString(logLine)
				}
                                totalValue = 0
				now = now.Add(time.Second)
				timeStr = now.Format("Jan 02 15:04:05")
				//fmt.Printf("\n\n")
			}

		}
		f.Close()
	}

	if len(cs) == 0 {
		return nil, fmt.Errorf("no chunks found")
	}

	if !silent {
		fmt.Fprintf(os.Stderr, "yang yazhou result:found %d samples\n", count)
	}

	if shouldMerge {
		total := map[string]ftdc.Metric{}
		for _, c := range cs {
			for _, m := range c.Metrics {
				k := m.Key
				if _, ok := total[k]; ok {
					// !! this expects contigious chunks
					newDeltas := make([]int, 0, len(total[k].Deltas)+len(m.Deltas))
					newDeltas = append(newDeltas, total[k].Deltas...)
					newDeltas = append(newDeltas, m.Deltas...)
					total[k] = ftdc.Metric{
						Key:    k,
						Value:  total[k].Value,
						Deltas: newDeltas,
					}
				} else {
					total[k] = m
				}
			}
		}
		return total, nil
	}

	maps := []map[string]ftdc.Metric{}
	for _, c := range cs {
		maps = append(maps, c.Map())
	}
	return cs, nil

}

func writeJSONtoFile(output interface{}, file string) error {
	of, err := os.OpenFile(file, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0666)
	if err != nil {
		return fmt.Errorf("failed to open write file '%s': %s", file, err)
	}
	defer of.Close()
	enc := json.NewEncoder(of)

	err = enc.Encode(output)
	if err != nil {
		return fmt.Errorf("failed to write output to '%s': %s", file, err)
	}
	return nil
}
