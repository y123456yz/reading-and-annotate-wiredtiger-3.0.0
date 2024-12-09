# FTDC-utils

This repository has a Go library and a command-line utility for using
full-time diagnostic data capture files.

The command-line utility, **`ftdc`**, helps you:
- **`decode`**   decode diagnostic files into raw JSON output
- **`stats`**    read diagnostic file(s) into aggregated statistical output
- **`compare`**  compare statistical output

# Installation

```sh
# for the ftdc command
go get github.com/10gen/ftdc-utils/cmd/ftdc
# for the Go library, imports as 'ftdc'
go get github.com/10gen/ftdc-utils
```

# Usage

## Decode

```
Usage:
  ftdc decode [OPTIONS] FILE...

        --start=<TIME>    clip data preceding start time (layout UnixDate)
        --end=<TIME>      clip data after end time (layout UnixDate)
    -m, --merge           merge chunks into one object
    -o, --out=<FILE>      write diagnostic output, in JSON, to given file
    -s, --silent          suppress chunk overview output
    FILE:                 diagnostic file(s)
```

## Stats

```
Usage:
  ftdc stats [OPTIONS] FILE...

        --start=<TIME>    clip data preceding start time (layout UnixDate)
        --end=<TIME>      clip data after end time (layout UnixDate)
    -o, --out=<FILE>      write stats output, in JSON, to given file
    FILE:                 diagnostic file(s)
```

## Compare

```
Usage:
  ftdc compare [OPTIONS] STAT1 STAT2

    -e, --explicit             show comparison values for all compared metrics;
                               sorted by score, descending
    -t, --threshold=<FLOAT>    threshold of deviation in comparison (default: 0.3)
    STAT1:                     statistical file (JSON)
    STAT2:                     statistical file (JSON)
```

