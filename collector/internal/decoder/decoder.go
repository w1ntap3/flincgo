// Package decoder best pkg btw
package decoder

import (
	"bytes"
	"encoding/binary"
	"fmt"
)

type Header struct {
	Magic      [4]byte
	Sequence   uint32
	Timestamp  uint64
	Severity   uint8
	ItemID     uint16
	PayloadLen uint16
}

type Log struct {
	Header  Header
	payload []byte
}

// Decode hi bro
func Decode(datagram []byte) (Log, error) {
	var l Log

	magicBytes := []byte{'F', 'C', 'G', 'O'}
	if !bytes.Equal(datagram[:4], magicBytes) {
		return Log{}, fmt.Errorf("invalid magic: %v does not match %v", datagram[:4], magicBytes)
	}
	copy(datagram[:4], l.Header.Magic[:])

	var sequence uint32
	n, err := binary.Decode(datagram[4:8], binary.LittleEndian, &sequence)
	if err != nil {
		return Log{}, fmt.Errorf("decoding sequence: %w", err)
	}

	if n != binary.Size(sequence) {
		return Log{}, fmt.Errorf("expected %v wrote %v bytes", binary.Size(sequence), n)
	}
	return l, nil
}
