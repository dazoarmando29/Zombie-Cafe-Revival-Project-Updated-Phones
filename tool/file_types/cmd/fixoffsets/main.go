package main

/* Repairs characterOffsetsN.bin.mid: the seasonal sheet regeneration
 * (gen_costumes.py era) lost per-piece trim offsets (packer logged
 * "continuing with no offsets" for missing .png.json sidecars). The
 * pristine baseline APK holds the original values. For every entry
 * where our file has a zero trim field and the original has a value,
 * restore the original value (keep OUR rects - they match our packing).
 *
 * Usage: fixoffsets <ours.bin> <orig.bin> <out.bin> */
import (
	"bytes"
	"fmt"
	"os"

	ft "file_types"
)

func main() {
	if len(os.Args) < 4 {
		fmt.Println("usage: fixoffsets <ours> <orig> <out>")
		os.Exit(1)
	}
	oursB, _ := os.ReadFile(os.Args[1])
	origB, _ := os.ReadFile(os.Args[2])
	ours := ft.ReadImageOffsets(bytes.NewReader(oursB))
	orig := ft.ReadImageOffsets(bytes.NewReader(origB))
	fmt.Printf("ours=%d entries, orig=%d entries\n", len(ours.Offsets), len(orig.Offsets))

	fixed := 0
	for i := range ours.Offsets {
		if i >= len(orig.Offsets) {
			break
		}
		o := &ours.Offsets[i]
		g := orig.Offsets[i]
		if o.Name != g.Name {
			fmt.Printf("WARN name mismatch at %d: ours=%q orig=%q (leaving ours)\n",
				i, o.Name, g.Name)
			continue
		}
		before := *o
		if o.XOffset == 0 && g.XOffset != 0 {
			o.XOffset = g.XOffset
		}
		if o.YOffset == 0 && g.YOffset != 0 {
			o.YOffset = g.YOffset
		}
		if o.XOffsetFlipped == 0 && g.XOffsetFlipped != 0 {
			o.XOffsetFlipped = g.XOffsetFlipped
		}
		if o.YOffsetFlipped == 0 && g.YOffsetFlipped != 0 {
			o.YOffsetFlipped = g.YOffsetFlipped
		}
		if *o != before {
			fixed++
		}
	}
	fmt.Printf("restored trim offsets on %d entries\n", fixed)

	var out bytes.Buffer
	ft.WriteImageOffsets(&out, ours)
	if err := os.WriteFile(os.Args[3], out.Bytes(), 0644); err != nil {
		panic(err)
	}
	fmt.Printf("wrote %s (%d bytes)\n", os.Args[3], out.Len())
}
