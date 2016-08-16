// topodb project main.go
package main

import (
    "fmt"
    "os"
    "net"
    "encoding/json"
    "encoding/binary"
)

var _topoJson []byte
var _topoJsonFile string
var _topoListenAddr string
var _topoDataVersion int
var _topoJsonRespProtocol []byte

func main() {
    if len(os.Args) < 3 {
        fmt.Println("less args, should be: 'topo json file' 'listen tcp addr'")
        return
    }
    
    _topoJsonFile = os.Args[1]
    _topoListenAddr = os.Args[2]
    
    LoadTopoJson()
    fmt.Println("loaded in topo json, length: ", len(_topoJson))
    if len(_topoJson) == 0 {
        fmt.Println("Invalid topo json")
        return
    }
    
    _topoDataVersion = 1
    _topoJsonRespProtocol = SerializeResponse(_topoDataVersion, string(_topoJson))

    tcpAddr, err := net.ResolveTCPAddr("tcp", _topoListenAddr)
    if err != nil {
        fmt.Println("listen address invalid: ", _topoListenAddr)
        return
    }

    listener, err := net.ListenTCP("tcp", tcpAddr)
    if err != nil {
        fmt.Println("failed to listen on: ", tcpAddr)
        return
    }

    for {
        tcpconn, err := listener.AcceptTCP()
        if err != nil {
            fmt.Println("listener accepted err: ", err)
            break
        } else {
            fmt.Println("accepted one connection from ", tcpconn.RemoteAddr().String())
            go ConnectionHandler(tcpconn)
        }
    }
    
    fmt.Println("topodb quit...")
}

/**
*
* actually we need to define specific protocol for topology db
* |one byte 0XFAAF|4byte json len| json|
*
* {
    "cmd":"data-request",
    "db-version":0
* }

* {
    "cmd":"data-resp",
    "db-version":1,
    "data":dbJson
* }
*
*/
func ConnectionHandler(tcpconn *net.TCPConn) {
    var buffer [512]byte
    readnum := 0

    tcpconn.SetNoDelay(true)
    
    for {
        readcnt, err := tcpconn.Read(buffer[readnum :])
        if err != nil {
            fmt.Println("reading connection err: ", err, ", from: ", tcpconn.RemoteAddr())
            break
        }

        readnum += readcnt
        if readnum >= 6 {
            if buffer[0] != 0xFA || buffer[1] != 0xAF {
                fmt.Println("protocol sync mark error from: ", tcpconn.RemoteAddr())
                break
            }

            jsonlen := binary.BigEndian.Uint32(buffer[2 :])
            if int(jsonlen) + 6 > readnum {
                continue
            }
            
            requestVersion := DeserializeRequest(buffer[6 : 6 + jsonlen])
            fmt.Println("got request with version: ", requestVersion, ", from: ", tcpconn.RemoteAddr())
            if requestVersion < 0 || requestVersion >= _topoDataVersion {
                break
            }
            
            // answer data
            fmt.Println("answer response with version: ", _topoDataVersion)
            tcpconn.Write(_topoJsonRespProtocol)
            break
        }
    }
    
    fmt.Println("closing connection from: ", tcpconn.RemoteAddr())
    tcpconn.Close()
}

func DeserializeRequest(buff []byte) (dataVersion int) {
    defer func() {
        if err := recover(); err != nil {
            dataVersion = 0
        }
    }()
    
    var jsonValue map[string]interface{}
    err := json.Unmarshal(buff, &jsonValue)
    if err != nil {
        return
    }

    cmd, ok := jsonValue["cmd"];
    if ok && cmd.(string) == "data-request" {
        dataVersion = int(jsonValue["db-version"].(float64))
    }
    return
}

func SerializeResponse(dataVersion int, jsonValue string) []byte {
    marsal := make(map[string] interface{})
    marsal["cmd"] = "data-resp"
    marsal["db-version"] = dataVersion
    marsal["data"] = jsonValue

    jsonbuf, _ := json.Marshal(marsal)

    bodylen := len(jsonbuf)
    buff := make([]byte, bodylen + 6)

    copy(buff[6 :], jsonbuf)
    buff[0] = 0xFA
    buff[1] = 0xAF
    binary.BigEndian.PutUint32(buff[2 :], uint32(bodylen))
    return buff
}

//// json
func LoadTopoJson() {
    fmt.Println("opening dbfile from: ", _topoJsonFile)
    f, err := os.Open(_topoJsonFile)
    if err != nil {
        fmt.Println("failed to open dbfile, err: ", err)
        return
    }

    fi, _ := f.Stat()
    buf := make([]byte, int(fi.Size()) + 12)
    n, err := f.Read(buf)
    if err != nil {
        fmt.Println("failed to read in db file, err: ", err)
        f.Close()
        return
    }

    f.Close()

    var jsonValue map[string] interface{}
    err = json.Unmarshal(buf[: n], &jsonValue)
    if err != nil {
        fmt.Printf("failed to load topo json: %s", err.Error())
        return
    }

    VerifyTopoJson()
    _topoJson = buf[ : n]
}

func VerifyTopoJson() {
    
}


