package echo

import (
	"bytes"
	"context"
	"net"
	"testing"
	"time"
)

// instantiate the echoserverudp
// create a client which listens to serverAddr via net.Dial (returns net.Conn)
func TestDialUDP(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	serverAddr, err := echoServerUDP(ctx, "127.0.0.1:")
	if err != nil {
		t.Fatal(err)
	}
	defer cancel()

	client, err := net.Dial("udp", serverAddr.String())
	if err != nil {
		t.Fatal(err)
	}
	defer client.Close()

	// the book starts this as a net.ListenPacket, but I see no reason. This is a one to one interloper that sends a message to the client.
	interloper, err := net.Dial("udp", client.LocalAddr().String())
	if err != nil {
		t.Fatal(err)
	}

	// write message to client via interloper
	// send messsage to echoserver via client (make sure it comes back)
	interrupt := []byte("this is an intrusive msg")
	_, err = interloper.Write(interrupt)
	// we dont check the amount of bytes written here because a UDP datagram is either fully sent or errors
	// ^ UDP is a datagram / message oriented protocol. All or nothing.
	if err != nil {
		t.Fatal(err)
	}
	interloper.Close()

	pingMsg := []byte("ping")
	_, err = client.Write(pingMsg)
	if err != nil {
		t.Fatal(err)
	}

	buf := make([]byte, 1024)

	// ik the strict ordering here
	n, err := client.Read(buf)
	if err != nil {
		t.Fatal(err)
	}
	if bytes.Equal(buf[:n], interrupt) {
		t.Fatal("wtf you read messages from the interloper, net.Dial doesnt work.")
	}
	if !bytes.Equal(buf[:n], pingMsg) {
		t.Fatal("expected %q got %q", pingMsg, buf[:n])
	}

	// INFO: I thought that the .Read method would still return the message from the interloper,
	// looking back on it, that'd ruin the point of the abstraction anyways. The code blocked here
	// so i just removed the comments

	/* n, err = client.Read(buf)
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(buf[:n], pingMsg) {
		t.Fatal("wtf you arent reading messages from the echo server, net.Dial doesn't work")
	}
	fmt.Println("hi2")
	*/

	err = client.SetDeadline(time.Now().Add(time.Second))
	if err != nil {
		t.Fatal(err)
	}

	// we do crazy amounts of error checking cuz this is a test
	if _, err := client.Read(buf); err == nil {
		t.Fatalf("unexpected packet")
	}
}
