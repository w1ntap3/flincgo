package echo

import (
	"bytes"
	"context"
	"fmt"
	"net"
	"os"
	"os/signal"
	"testing"
)

// setup a context with a cancel, so that we can cancel and end the socket connection at the end of our function
// call the echoserver udp, which gives us the address of the server and an error if something goes wrong
func TestEchoServerUDP(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	// the net.Addr interface has 2
	serverAddr, err := echoServerUDP(ctx, "127.0.0.1:")
	if err != nil {
		t.Fatal(err)
	}
	defer cancel()

	client, err := net.ListenPacket("udp", "127.0.0.1:")
	if err != nil {
		t.Fatal(err)
	}
	defer func() { _ = client.Close() }()

	msg := []byte("ping")
	_, err = client.WriteTo(msg, serverAddr)
	if err != nil {
		t.Fatal(err)
	}

	buf := make([]byte, 1024)
	n, addr, err := client.ReadFrom(buf)
	if err != nil {
		t.Fatal(err)
	}

	if addr.String() != serverAddr.String() {
		t.Fatalf("recieved reply frmo %q instead of %q", addr, serverAddr)
	}

	if !bytes.Equal(msg, buf[:n]) {
		t.Errorf("expected %q; actual reply %q", msg, buf[:n])
	}
}

// INFO: this is my own addition
func TestEchoServerUDPInteractive(t *testing.T) {
	// this is a special type of context.WithCancel which cancels whenever signals... is pressed.
	// this stops the function from blocking on ctx.done.
	// SIGINT is the unix version of os.Interrupt.
	// whenever you press ctrl+c the kernel sends SIGINT to the foreground process group of the shell (IDK WTF THAT IS LOL)
	// SIGTERM is when you try to kill it via the `kill` command in unix systems. the kernel sends the foreground process group SIGKILL.
	//
	ctx, cancel := signal.NotifyContext(
		context.Background(),
		os.Interrupt,
	)
	defer cancel()

	serverAddr, err := echoServerUDP(ctx, "127.0.0.1:")
	if err != nil {
		t.Fatal(err)
	}

	t.Logf("server listening on %q", serverAddr)
	<-ctx.Done()
	fmt.Println("server stopped")
}
