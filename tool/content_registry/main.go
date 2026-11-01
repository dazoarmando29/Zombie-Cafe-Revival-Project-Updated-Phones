// content_registry: stable content identifiers and append-only enforcement
// for Zombie Café Revival data files.
//
// Usage:
//
//	go run ./tool/content_registry -root src/assets init   # assign missing ids + lock
//	go run ./tool/content_registry -root src/assets check  # validate (build gate)
//	go run ./tool/content_registry -root src/assets lock   # regenerate lockfile
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"

	"content_registry/registry"
)

func main() {
	root := flag.String("root", "src/assets", "assets root (contains data/)")
	flag.Parse()

	mode := flag.Arg(0)
	if mode == "" {
		mode = "check"
	}

	dataDir := filepath.Join(*root, "data")
	lockPath := filepath.Join(dataDir, "zcr_registry.lock.json")

	switch mode {
	case "init":
		n, res := registry.AssignMissingIDs(dataDir)
		for _, is := range res.Issues {
			fmt.Println(is)
		}
		fmt.Printf("content_registry: assigned %d id(s)\n", n)
		lf, res2 := registry.BuildLock(dataDir)
		for _, is := range res2.Issues {
			fmt.Println(is)
		}
		if err := registry.SaveLock(lockPath, lf); err != nil {
			fmt.Println("content_registry: lock write:", err)
			os.Exit(1)
		}
		fmt.Printf("content_registry: lockfile %s (%d entries)\n", lockPath, len(lf.Entries))

	case "lock":
		lf, res := registry.BuildLock(dataDir)
		for _, is := range res.Issues {
			fmt.Println(is)
		}
		if res.Errors() > 0 {
			os.Exit(1)
		}
		if err := registry.SaveLock(lockPath, lf); err != nil {
			fmt.Println("content_registry: lock write:", err)
			os.Exit(1)
		}
		fmt.Printf("content_registry: lockfile %s (%d entries)\n", lockPath, len(lf.Entries))

	case "check":
		res := registry.Validate(dataDir, lockPath)
		for _, is := range res.Issues {
			fmt.Println(is)
		}
		fmt.Printf("content_registry: %d error(s)\n", res.Errors())
		if res.Errors() > 0 {
			os.Exit(1)
		}

	default:
		fmt.Println("unknown mode:", mode, "(want init|lock|check)")
		os.Exit(2)
	}
}
