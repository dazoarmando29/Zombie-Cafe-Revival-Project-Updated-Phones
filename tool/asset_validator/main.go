// CLI: validate Zombie Cafe asset sources before building the APK.
//
//	go run ./tool/asset_validator -root src/assets
//	go run ./tool/asset_validator -root src/assets -strict
//
// Exit code 1 if any [error] is found (or any [warn] with -strict).
package main

import (
	"flag"
	"fmt"
	"os"

	"asset_validator/validator"
)

func main() {
	root := flag.String("root", "src/assets", "asset root directory (contains data/ and images/)")
	strict := flag.Bool("strict", false, "treat warnings as errors")
	flag.Parse()

	if st, err := os.Stat(*root); err != nil || !st.IsDir() {
		fmt.Fprintf(os.Stderr, "asset_validator: root %q is not a directory\n", *root)
		os.Exit(2)
	}

	res := validator.ValidateAssets(*root)
	for _, is := range res.Issues {
		fmt.Println(is)
	}
	errs := res.Errors()
	warns := len(res.Issues) - errs
	fmt.Printf("asset_validator: %d error(s), %d warning(s)\n", errs, warns)
	if errs > 0 || (*strict && warns > 0) {
		os.Exit(1)
	}
}
