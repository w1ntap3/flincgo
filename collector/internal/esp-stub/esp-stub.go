// Package espstub this is a mock esp i use for development and debugging. sends an unencrypted copy of the c struct
package espstub

import (
	"encoding/binary"
	"fmt"
	mrand "math/rand/v2"
	"net"
	"time"
)

func MockEdge(addr string) error {
	// hardcoded for now
	s, err := net.Dial("udp", addr)
	fmt.Println("dialed the network")
	if err != nil {
		fmt.Println("erorr here connecting to address", err)
		return fmt.Errorf("connecting to addr %s: %w", addr, err)
	}

	go func() {
		for {
			time.Sleep(time.Second * 2)
			stub := stubDatagram()
			_, err = s.Write(stub)
			fmt.Println("wrote stub datagram %s", string(stub))
			if err != nil {
				fmt.Println(err)
				return
			}
		}
	}()

	return nil
}

func stubDatagram() []byte {
	payload := make([]byte, 1400)

	const headerSize = 21
	buf := make([]byte, headerSize+len(payload))

	copy(buf[0:4], []byte{'F', 'C', 'G', 'O'})

	// AI GENERATED:
	// TODO: understand the binary lib better
	// Sequence number: uniquely identifies/orders this message.
	binary.LittleEndian.PutUint32(buf[4:8], 42)

	// Timestamp: when the message was created.
	binary.LittleEndian.PutUint64(buf[8:16], 8_421_337)

	// Severity: single byte, so no byte-order conversion is needed.
	buf[16] = 2

	// Item ID: identifies the type/source of the logged item.
	binary.LittleEndian.PutUint16(buf[17:19], 7)

	// Payload length: tells the decoder how many payload bytes follow the header.
	binary.LittleEndian.PutUint16(buf[19:21], uint16(len(payload)))

	copy(buf[21:], payload)

	return buf
}

var words = [...]string{
	"segfault",
	"spaghetti",
	"goku",
	"broly",
	"rogue lineage",
	"pikachu",
	"yugioh",
	"deadlock",
	"oom",
	"forkbomb",
	"goblin",
	"kernelpanic",
	"racecondition",
	"techdebt",
	"works-on-my-machine",
	"undefined-behavior",
	"sudo",
	"daemon",
	"localhost",
	"nullpointer",
	"yolo-deploy",
	"hotfix",
	"legacy",
	"microservice",
	"monad",
	"regex",
	"kubectl",
	"rm-rf",
	"off-by-one",
}

func randomWord() string {
	return words[mrand.IntN(len(words))]
}
