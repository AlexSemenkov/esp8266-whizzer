h1 Description
=====================
Whizzer is IoT device designed to emulate human walking on smart sneakers with pace counter.

h3 Its features:

h5 1. Aimed at automation QA testing
h5 2. Remote connection via HTTP protocol
h5 3. Allows to emulate desired amount of steps for further verification
h5 4. Control a duration and speed over the top of a single step

h1 Installation
h3relevant to Windows 7/8/10

1. Download and install custom [Espressif-ESP8266-DevKit-v2.2.1-x86](http://dl.programs74.ru/get.php?file=EspressifESP8266DevKit) (by Mikhail Grigoriev)
2. Download and install [Java Runtime](http://www.oracle.com/technetwork/java/javase/downloads/index.html) x86 or x64
3. Download [C/C++ Eclipse IDE](https://eclipse.org/downloads/packages/eclipse-ide-cc-developers/oxygenr) and install it in root folder C:\
4. Download [MinGW](http://sourceforge.net/projects/mingw/files/Installer/). Run mingw-get-setup.exe without GUI: uncheck "...also install support for the graphical user interface"
5. Download and run custom pack of [scripts for MinGW modules installation](http://dl.programs74.ru/get.php?file=EspressifESP8266DevKitAddon) (by Mikhail Grigoriev)

h1 Connection to Internet

1. Device initial state is 'SOFT_AP' mode
2. Connect PC to the softAP (SSID for SOFT_AP is 'ESP_XXXXXX', no password)
3. Send a command to make ESP8266 station connect to router and reboot the device
	'URL:			http://192.168.4.1/config?command=wifi
	Method:			POST
	Content-Type:	application/json
	Body: 			{ "Request": 
						{ "Station":
							{ "Connect_Station": 
								{ "ssid": "%ssid%",
								  "password": "%password%",
								  "token": "%token%" 
								}
							}
						}
					}'
4. Connected to router, the device tries to communicate with [IoT·Espressif cloud server](http://iot.espressif.cn) for authentication
5. If everything is OK, mode should be changed to 'STATION', router's ssid and password are cashed (no need to repeat this procedure again)

h1 Communication with Whizzer

h5 Performed via "IoT·Espressif" cloud service '<http://iot.espressif.cn>'

* **URL**
	'/v1/device/rpc'

* **Method**
	'GET'

* **Authorization**
	'token' - Owner key, which can be found only in Device Account area of "IoT·Espressif" cloud

* **URL Params**
	
	* **Required:**
	  action=[ 'sys_upgrade' | 'sys_reboot' | 'emulate_steps ']
	
	* **Optional (if action=emulate_steps):**
	  steps_num=['integer']
	  pwm_cycle_decay_stop=['integer']

* **Success Response**

    Code: 200
    Content:  { "status": 200,
				"emulating_steps": {
					"steps_num": 10,
					"pwm_cycle_decay_stop": 144 },
				"nonce": 127915383,
				"deliver_to_device": true }
	Description: Request is successfully delivered to the device and it has started step emulation process
	
* **Error Response**
	
	* **Code:** 500
	  **Content:** '{ "message": "devicekey not exists", "status": 500 }'
	  **Description:** Authorization token (device owner key) is wrong
	
	* **Code:** 404
	  **Content:** '{ "message": "remote peer is disconnected", "status": 404 }'
	  **Description:** the device is either disconnected or in sleep mode
	
	* **Code:** 400
	  **Content:** '{ "code": 400, "message": "invalid value of steps number (PWM cycle decay stop)" }'
	  **Description:** steps number (PWM cycle decay stop) must be unsigned integer greater than zero
	
	* **Code:** 422
	  **Content:** '{ "code": 422, "message": "steps (PWM cycle decay) threshold exceeded: greater than %d" }'
	  **Description:** steps (PWM cycle decay) value must not be extremely large
	
	* **Code:** 422
	  **Content:** '{ "code": 422, "message": "short of steps (PWM cycle decay) value: less than %d" }'
	  **Description:** steps (PWM cycle decay) value must not be too small

	* **Code:** 423
	  **Content:** '{ "code": 423, "message": "locked, whizzer is busy - steps left: %d" }'
	  **Description:** the device is emulating steps at the moment

* Sample Call via Windows CURL
	'curl -X GET -H 
	"Content-Type:application/json" 
	-H "Authorization: token %token%" 
	"http://iot.espressif.cn/v1/device/rpc/?deliver_to_device=true&action=emulate_steps&steps_num=10&pwm_cycle_decay_stop=144"'
