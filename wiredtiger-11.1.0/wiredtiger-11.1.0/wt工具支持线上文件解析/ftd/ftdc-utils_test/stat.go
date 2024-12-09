package ftdc

import (
	"io"
	"math"
	"sync"
	"time"
)

// MetricStat represents basic statistics for a single metric
type MetricStat struct {
	// Avg is the mean of the metric's deltas. It is analogous to the first
	// derivative.
	Avg int

	// Var is the variance. It is related to the absolute second derivative.
	Var int
}

// Stats represents basic statistics for a set of metric samples.
type Stats struct {
	Start    time.Time
	End      time.Time
	Metrics  map[string]MetricStat
	NSamples int
}

// Stats produces Stats for the Chunk
func (c *Chunk) Stats() (s Stats) {
	s.NSamples = 1 + c.NDeltas
	s.Metrics = make(map[string]MetricStat)
	var start, end int
	for _, m := range c.Metrics {
		s.Metrics[m.Key] = computeMetricStat(m)
		if m.Key == "start" {
			start = m.Value / 1000
			end = (m.Value + sum(m.Deltas...)) / 1000
		}
	}
	s.Start = time.Unix(int64(start), 0)
	s.End = time.Unix(int64(end), 0)
	return
}

// ComputeAllChunkStats takes an FTDC diagnostic file in the form of an
// io.Reader, and computes statistics for all metrics on each chunk.
func ComputeStats(r io.Reader) (cs []Stats, err error) {
	ch := make(chan Chunk)
	wg := new(sync.WaitGroup)
	wg.Add(1)
	go func() {
		for c := range ch {
			cs = append(cs, c.Stats())
		}
		wg.Done()
	}()
	err = Chunks(r, ch)
	if err != nil {
		return
	}
	wg.Wait()
	return
}

// ComputeStatsInterval takes an FTDC diagnostic file in the form of an
// io.Reader, and computes statistics for all metrics within the given time
// frame, clipping chunks to fit.
func ComputeStatsInterval(r io.Reader, start, end time.Time) (cs []Stats, err error) {
	ch := make(chan Chunk)
	wg := new(sync.WaitGroup)
	wg.Add(1)
	go func() {
		for c := range ch {
			if c.Clip(start, end) {
				cs = append(cs, c.Stats())
			}
		}
		wg.Done()
	}()
	err = Chunks(r, ch)
	if err != nil {
		return
	}
	wg.Wait()
	return
}

// MergeStats computes a time-weighted merge of Stats.
func MergeStats(cs ...Stats) (m Stats) {
	var start int64 = math.MaxInt64
	var end int64 = math.MinInt64
	weights := make([]int, len(cs))
	avgs := make(map[string][]int)
	vars := make(map[string][]int)
	for i, s := range cs {
		m.NSamples += s.NSamples
		sStart := s.Start.Unix()
		sEnd := s.End.Unix()
		if sStart < start {
			start = sStart
		}
		if sEnd > end {
			end = sEnd
		}
		weights[i] = int(sEnd - sStart)
		for k, v := range s.Metrics {
			if _, ok := avgs[k]; !ok {
				avgs[k] = make([]int, len(cs))
				vars[k] = make([]int, len(cs))
			}
			avgs[k][i] = v.Avg
			vars[k][i] = v.Var
		}
	}
	m.Start = time.Unix(start, 0)
	m.End = time.Unix(end, 0)
	m.Metrics = make(map[string]MetricStat)
	for k := range avgs {
		avg := weightedAvg(avgs[k], weights)
		variance := weightedVar(avg, avgs[k], vars[k], weights)
		m.Metrics[k] = MetricStat{
			Avg: avg,
			Var: variance,
		}
	}
	return
}

func computeMetricStat(m Metric) MetricStat {
	if len(m.Deltas) == 0 {
		return MetricStat{-1, -1}
	}
	l := make([]int, len(m.Deltas))
	copy(l, m.Deltas)
	avg := sum(l...) / len(l)
	var variance int
	for _, x := range l {
		variance += square(x - avg)
	}
	variance /= len(l)
	return MetricStat{
		Avg: avg,
		Var: variance,
	}
}

func weightedAvg(l, w []int) (v int) {
	W := 0
	for i := range w {
		v += w[i] * l[i]
		W += w[i]
	}
	v /= W
	return
}

func weightedVar(avg int, avgs, vars, w []int) (v int) {
	W := 0
	for i := range w {
		v += w[i] * (vars[i] + square(avgs[i]-avg))
		W += w[i]
	}
	v /= W
	return
}
