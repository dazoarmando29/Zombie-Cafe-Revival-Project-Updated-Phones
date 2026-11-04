package main

/* Dumps Zombie Cafe save files (SaveGame .dat) with byte offsets so the
 * CharacterInstance.Type values can be patched in place later.
 * Usage: go run ./tool/file_types/cmd/dumpsave <file.dat> */
import (
	"bytes"
	"fmt"
	"os"

	ft "file_types"
)

type countR struct {
	r *bytes.Reader
	n int64
}

func (c *countR) Read(p []byte) (int, error) {
	n, err := c.r.Read(p)
	c.n += int64(n)
	return n, err
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("usage: dumpsave <save.dat> | dumpsave --db <characterData.bin.mid>")
		os.Exit(1)
	}
	if os.Args[1] == "--db" {
		data, err := os.ReadFile(os.Args[2])
		if err != nil {
			panic(err)
		}
		rows := ft.ReadCharacters(bytes.NewReader(data))
		fmt.Printf("db=%s rows=%d\n", os.Args[2], len(rows))
		for i, r := range rows {
			fmt.Printf("row[%3d]: name=%q head=%q art=%q female=%v\n",
				i, r.Name, r.CharacterArtStringHead, r.CharacterArtString, r.IsFemale)
		}
		return
	}
	if os.Args[1] == "--offsets" {
		data, err := os.ReadFile(os.Args[2])
		if err != nil {
			panic(err)
		}
		off := ft.ReadImageOffsets(bytes.NewReader(data))
		fmt.Printf("offsets=%s type=%d count=%d\n", os.Args[2], off.Type, len(off.Offsets))
		for i, o := range off.Offsets {
			bad := ""
			if o.W == 0 || o.H == 0 {
				bad = " ZERO-RECT"
			}
			if o.Name == "" {
				bad += " NO-NAME"
			}
			fmt.Printf("idx[%4d]: name=%q rect=(%d,%d %dx%d) off=(%d,%d f:%d,%d)%s\n",
				i, o.Name, o.X, o.Y, o.W, o.H, o.XOffset, o.YOffset,
				o.XOffsetFlipped, o.YOffsetFlipped, bad)
		}
		return
	}
	data, err := os.ReadFile(os.Args[1])
	if err != nil {
		panic(err)
	}
	fmt.Printf("file=%s size=%d\n", os.Args[1], len(data))

	cr := &countR{r: bytes.NewReader(data)}
	// version byte
	ver := make([]byte, 1)
	cr.Read(ver)
	fmt.Printf("version=%d\n", ver[0])

	// re-parse manually around the library to record Type offsets
	cr2 := &countR{r: bytes.NewReader(data)}
	save := ft.ReadSaveGame(cr2)
	st := save.State
	fmt.Printf("avatar: type=%d name=%q\n", st.Character.Type, st.Character.Name)
	fmt.Printf("numZombies=%d\n", st.NumZombies)
	for i, z := range st.Zombies {
		fmt.Printf("zombie[%d]: type=%3d name=%q U2=%d U3=%d U5=%d U7=%d U10=%d U11=%d U12=%d U13=%d U14=%d U15=%d U16=%d\n",
			i, z.Type, z.Name, z.U2, z.U3, z.U5, z.U7, z.U10, z.U11, z.U12, z.U13, z.U14, z.U15, z.U16)
	}
	fmt.Printf("money=%d toxin=%d level=%d xp=%v\n", st.Money, st.Toxin, st.Level, st.ExperiencePoints)
}
