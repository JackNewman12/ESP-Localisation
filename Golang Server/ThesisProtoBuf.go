package main

import "net"
import "fmt"
import "time"
import "bufio"
import "os"
import "os/exec"
import "runtime"
import "encoding/binary"

import (
	"./ThesisProtocol"
	"github.com/golang/protobuf/proto"
)

// @fixme: Just import this from the ProtoBuf Generator
type CurrentDevices struct {
	DeviceX       int32
	DeviceY       int32
	DeviceVoltage float32
	LastData      time.Time
	PostDelay     int32
	LastPost      bool
}

// Global copy of the tracked devices
// Probably should have just used a channel to send this info and let another
// class figure out how to sort it
var currentDevices = []CurrentDevices{}
var file *os.File
var err error

// Go Worker for writing the log files
func writer() {
	file, err = os.OpenFile("output.txt", os.O_APPEND, 777)
	if err != nil {
		file, err = os.Create("output.txt")
	}

	w := bufio.NewWriterSize(file, 100000000) //100MB Cache
	for {
		if len(convertedData) == 0 { //While there is nothing else to do, make sure we write everything to the log
			w.Flush()
			time.Sleep(time.Millisecond)
		}

		deviceData := <-convertedData
		devicesData := deviceData.GetDevices()

		for device := range devicesData {
			dataPoints := devicesData[device].GetDatapoints()
			for point := range dataPoints {
				if dataPoints[point].GetRssi() != 0 { //Blank Info

					w.WriteString(fmt.Sprintf("%d %d %02X:%02X:%02X:%02X:%02X:%02X %d %d\r\n", deviceData.GetDeviceX(), deviceData.GetDeviceY(), devicesData[device].GetMac()[0], devicesData[device].GetMac()[1], devicesData[device].GetMac()[2], devicesData[device].GetMac()[3], devicesData[device].GetMac()[4], devicesData[device].GetMac()[5], dataPoints[point].GetRssi(), time.Now().Add(-1*time.Second*time.Duration(dataPoints[point].GetSecondsSince()+deviceData.GetSubmitTime())).Unix()))
				}
			}
		}

		var newDeviceFound bool = true
		for i := range currentDevices {
			// deviceInfo := currentDevices[i]
			if currentDevices[i].DeviceX == deviceData.GetDeviceX() && currentDevices[i].DeviceY == deviceData.GetDeviceY() { //If we find the same device, update its values
				currentDevices[i].DeviceVoltage = deviceData.GetDeviceVoltage()
				currentDevices[i].LastPost = deviceData.GetLastPost()
				currentDevices[i].LastData = time.Now().Add(time.Duration(*deviceData.WifiTime) * time.Millisecond * -1)
				currentDevices[i].PostDelay = *deviceData.WifiTime
				newDeviceFound = false
				break
			}
		}
		if newDeviceFound {
			newDevice := new(CurrentDevices)
			newDevice.DeviceX = deviceData.GetDeviceX()
			newDevice.DeviceY = deviceData.GetDeviceY()
			newDevice.DeviceVoltage = deviceData.GetDeviceVoltage()
			newDevice.LastPost = deviceData.GetLastPost()
			newDevice.LastData = time.Now().Add(time.Duration(*deviceData.WifiTime) * time.Millisecond * -1)
			newDevice.PostDelay = *deviceData.WifiTime
			currentDevices = append(currentDevices, *newDevice)
		}
		// 		//time.Sleep(time.Second / 5)
	}

}

// Channel to store data directly from tcpworkers. Used by Coverted go routine
var convertedData = make(chan ThesisProtocol.ReceivedMessage, 10000)

// Worker to handle tcp connections and to extract the Protobuf Data
func tcpworker(conn net.Conn) {
	r := bufio.NewReaderSize(conn, 20000) //20KB Cache should be large enough
	defer conn.Close()                    //Close connection once this thread is complete

	var message []byte
	var messageSizeRaw []byte
	for len(messageSizeRaw) < 5 { //Grab the first 5 bytes
		conn.SetDeadline(time.Now().Add(time.Second * 3))
		p, err := r.ReadByte()
		if err != nil {
			//conn.Close()
			fmt.Println(err)
			failedTransmission++
			return
		} //Read till our first Delim
		messageSizeRaw = append(messageSizeRaw, p)
	}

	messageSize := binary.BigEndian.Uint16(messageSizeRaw)  //First two bytes tell us the size of the data coming
	submissionTime := messageSizeRaw[2]                     //Third Byte tell us how long the WeMoS took to connect to WiFi so we can adjust for it later
	wifiTime := binary.BigEndian.Uint16(messageSizeRaw[3:]) //The last two bytes tell us how long to adjust the time so everything stays in sync
	//fmt.Println("Submission Time:", submissionTime)
	for {
		//r.Read(message
		conn.SetDeadline(time.Now().Add(time.Second * 3))
		p, err := r.ReadByte()
		if err != nil {
			//conn.Close()
			fmt.Println(err)
			failedTransmission++
			return //something went bad, just kill this thread
		}
		message = append(message, p)
		if uint16(len(message)) == messageSize {
			//conn.Close()
			break
		}
	}

	testStuff := new(ThesisProtocol.ReceivedMessage) //Since protobuf is 5x faster than JSON. We can just do the unmarshaling in here instead of having a dedicated thread for it
	err = proto.Unmarshal(message, testStuff)
	if err != nil {

		fmt.Println(err)
		// 	fmt.Println(messageSize)
		// 	fmt.Println(messageSizeRaw)

		incorrectDataFormat++
		return
	}

	testStuff.SubmitTime = new(int32)
	*testStuff.SubmitTime = int32(submissionTime)

	testStuff.WifiTime = new(int32)
	*testStuff.WifiTime = int32(wifiTime)
	convertedData <- *testStuff
	correctTransmission++
	return

}

// Pretty print the noise generting WeMoS's
func printCurrentNoiseMakers() {
	fmt.Println("\n--Noise Makers--\n")
	for _, device := range noiseMakers {
		MAC := []byte(device.MAC)
		fmt.Println(fmt.Sprintf("%02X:%02X:%02X:%02X:%02X:%02X\t %.1fv", MAC[0], MAC[1], MAC[2], MAC[3], MAC[4], MAC[5], device.battery))
	}

}

// Pretty print the tracking / sniffing devices
func printcurrentdevices() { //Pretty GUI for currently tracked devices
	fmt.Println("\n--Devices--\n")
	for i := range currentDevices {
		deviceInfo := currentDevices[i]
		fmt.Printf("Device: %d %d", deviceInfo.DeviceX, deviceInfo.DeviceY)
		if deviceInfo.DeviceVoltage > 4.3 {
			fmt.Printf("  Voltage: USB")
		} else {
			fmt.Printf("  Voltage: %.2f %.0f%%", deviceInfo.DeviceVoltage, (deviceInfo.DeviceVoltage-3.7)*200)
		}
		if deviceInfo.LastPost {
			fmt.Print("   ***DEAD***")
		}
		fmt.Print("\t\t", time.Since(deviceInfo.LastData))
		fmt.Print("\t (", deviceInfo.PostDelay, ")")
		fmt.Print("\n")
	}

}

var correctTransmission = 0
var failedTransmission = 0
var incorrectDataFormat = 0
var numNoiseWorkers = 0

// Basic gui to ensure things havent gone bad
func gui() {
	time.Sleep(time.Second * 5)
	for {
		fileStats, _ := file.Stat()
		cmd := exec.Command("cmd", "/c", "cls")
		cmd.Stdout = os.Stdout
		cmd.Run()
		// fmt.Println("~Jack Newman Dataserver\nSize of Convert Queue: ", len(incomingData), "\nSize of Writing Queue: ", len(convertedData), "\nSize of Log File: ", fileStats.Size()/1000000, "MB", "\nNumber of threads: ", runtime.NumGoroutine())
		fmt.Println("~~Jack Newman Datacollector~~\n", "\nSize of Writing Queue: ", len(convertedData), "\nSize of Log File: ", float64(fileStats.Size()/1000000), "MB", "\nNumber of threads: ", runtime.NumGoroutine())
		fmt.Println("\nSuccessful Transmissions: ", correctTransmission, "\nFailed Transmissions: ", failedTransmission, "\nFailed Data Conversions: ", incorrectDataFormat, "\nNoise Workers: ", numNoiseWorkers)
		printCurrentNoiseMakers()
		printcurrentdevices()
		time.Sleep(time.Second / 3)
	}
}

type NoiseMaker struct {
	MAC     string
	battery float32
}

var noiseMakers []NoiseMaker

// For the connected devices that are just a dummy WeMoS's designed to just
// generate WiFi traffic (Easier plotting and smaller sample times)
func noiseworker(conn net.Conn) {
	defer func() { numNoiseWorkers-- }()
	defer conn.Close()
	numNoiseWorkers++

	r := bufio.NewReader(conn) //20KB Cache should be large enough

	MAC, err := r.ReadBytes('@')
	if err != nil {
		return
	}
	defer func() {
		for z, a := range noiseMakers {
			if a.MAC == string(MAC) {
				noiseMakers = append(noiseMakers[:z], noiseMakers[z+1:]...)
				break
			}
		}
	}()
	isNewDevice := true
	for _, a := range noiseMakers {
		if a.MAC == string(MAC) {
			isNewDevice = false
			break
		}
	}
	if isNewDevice {
		fmt.Println("Noise Added:", MAC)
		meme := new(NoiseMaker)
		meme.MAC = string(MAC)
		noiseMakers = append(noiseMakers, *meme)
	}

	for {
		conn.SetDeadline(time.Now().Add(time.Second))
		_, err := r.ReadBytes('s')
		if err != nil {
			fmt.Println("Noise Disconnected")
			return
		}

		deviceVoltage, err := r.ReadByte()
		if err != nil {
			fmt.Println("Noise Disconnected")
			return
		}
		for z, a := range noiseMakers {
			if a.MAC == string(MAC) {
				noiseMakers[z].battery = float32(deviceVoltage) / 10
				break
			}
		}
	}
}

var devicesToStart = make(chan net.Conn, 200)
var stopListening = false

// Used to sync the start of all devices.
// Useful so things are not logging to the server while still setting up a test
func startDevices() {

	ln, _ := net.Listen("tcp", ":54000")
	go deviceStarterListener(ln)

	reader := bufio.NewReader(os.Stdin)
	fmt.Println("Press Enter When Ready To Start")
	reader.ReadString('\n')
	close(devicesToStart)
	for elem := range devicesToStart {
		//time.Sleep(time.Second)
		_, err := elem.Write([]byte("GoGoGoGoGoGoGoGoGo\n"))
		if err != nil {
			fmt.Println(err)
		}
		elem.Close()
		fmt.Println("Start")
	}
	stopListening = true
	ln.Close()
}

// Deligate the tcpworkers for the sniffers
func deviceStarterListener(ln net.Listener) {
	for {
		conn, err := ln.Accept() // run loop forever (or until ctrl-c)
		if err != nil {
			if stopListening {
				return
			}
			fmt.Println(err)
		}
		devicesToStart <- conn
		fmt.Println("Devices Waiting", len(devicesToStart))

	}

}

// Deligate the tcpworkers for the noise generators
func noiseListener() {
	ln, _ := net.Listen("tcp", ":50000") //The Port that is used by the noise makers
	for {
		conn, err := ln.Accept()
		if err != nil {
			fmt.Println(err)
		}

		go noiseworker(conn)
	}
}

// Start the server and all the needed go routines.
func main() {
	fmt.Println("Launching server...")
	go writer()
	time.Sleep(time.Second / 2) //allows for the writer thread to create or open the log file
	go noiseListener()

	startDevices()
	go gui()

	ln, _ := net.Listen("tcp", ":52000") // Port for normal Submitting
	for {
		conn, err := ln.Accept() // run loop forever (or until ctrl-c)
		if err != nil {
			fmt.Println(err)
		}

		go tcpworker(conn)
	}

	// for {
	// 	time.Sleep(time.Second)
	// }

}
