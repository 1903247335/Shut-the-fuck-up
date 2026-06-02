package main

import (
	"encoding/json"
	"math/rand"
	"net/http"
	"strconv"
	"time"
)

var (
	INTERVAL_START = 30
	INTERVAL_END   = 60
	INIT_TIME      = 600
	STATE          = "start"
)

func writeJSON(w http.ResponseWriter, data interface{}) {
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(data)
}

func initHandler(w http.ResponseWriter, r *http.Request) {
	writeJSON(w, map[string]int{
		"init_time": INIT_TIME,
	})
}

func getIntervalHandler(w http.ResponseWriter, r *http.Request) {

	value := rand.Intn(INTERVAL_END-INTERVAL_START) + INTERVAL_START

	writeJSON(w, map[string]int{
		"interval": value,
	})
}

func getStateHandler(w http.ResponseWriter, r *http.Request) {

	writeJSON(w, map[string]string{
		"state": STATE,
	})
}

func setStateHandler(w http.ResponseWriter, r *http.Request) {

	state := r.URL.Query().Get("state")

	if state != "start" && state != "stop" {

		writeJSON(w, map[string]string{
			"msg": "error",
		})
		return
	}

	STATE = state

	writeJSON(w, map[string]string{
		"msg":   "success",
		"state": STATE,
	})
}

func setIntervalHandler(w http.ResponseWriter, r *http.Request) {

	startStr := r.URL.Query().Get("start")
	endStr := r.URL.Query().Get("end")

	start, err1 := strconv.Atoi(startStr)
	end, err2 := strconv.Atoi(endStr)

	if err1 != nil || err2 != nil {

		writeJSON(w, map[string]string{
			"msg": "error",
		})
		return
	}

	INTERVAL_START = start
	INTERVAL_END = end

	writeJSON(w, map[string]interface{}{
		"msg":   "success",
		"start": start,
		"end":   end,
	})
}

func main() {

	rand.Seed(time.Now().UnixNano())

	http.HandleFunc("/init", initHandler)
	http.HandleFunc("/get_interval", getIntervalHandler)
	http.HandleFunc("/get_state", getStateHandler)
	http.HandleFunc("/set_state", setStateHandler)
	http.HandleFunc("/set_interval", setIntervalHandler)

	http.ListenAndServe(":8000", nil)
}