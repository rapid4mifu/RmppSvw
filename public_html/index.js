import {setCallbackMessage, sendWebSocketData} from './socket.js';

var duty_slider = document.getElementById('out-duty');
noUiSlider.create(duty_slider, {
	start: [0],
	connect: true,	
	direction: 'rtl', // Put '0' at the bottom of the slider
	orientation: 'vertical',
	behaviour: 'tap',
	range: {
		'min': 0,
		'max': 100
	},
	pips: {mode: 'count', values: 6, density: 5}
});

var bytes_pre = new Uint8Array(4);
var mode;
var dir = 0;
var xctrl = false;
var aliveTimer = null;

const parseSokeck = (bytes) => {
	if (0x04 == bytes[0]) {
		resetAliveTimer();

		// byte index : 1
		if (bytes_pre[1] != bytes[1]) {
			// モード
			mode = bytes[1] & 0x0F;
			//console.info('mode : ' + mode);

			// 進行方向
			dir = (bytes[1] & 0x30) >> 4;
			//console.info('dir : ' + dir);
			switch (dir) {
				case 1:
					document.getElementById("out-fwd").style.fill = 'green';
					document.getElementById("out-rvs").style.fill = 'currentColor';
					break;
				case 2:
					document.getElementById("out-fwd").style.fill = 'currentColor';
					document.getElementById("out-rvs").style.fill = 'green';
					break;
				default:
					document.getElementById("out-fwd").style.fill = 'currentColor';
					document.getElementById("out-rvs").style.fill = 'currentColor';
			}
		}
		bytes_pre[1] = bytes[1];

		// 外部通信制御
		if (bytes[2] & 0x01) {
			// 外部通信制御：有効
			if (false == xctrl) {
				xctrl = true;
				console.info("external control is enable.");

				// 外部通信制御 定期処理
				const xctrlTImer = setInterval(() => {
					if (xctrl) {
						console.count("update_speed");
						update_speed();
					} else {
						console.countReset("update_speed");
						clearInterval(xctrlTImer);
					}
				}, 200);
			}
		} else {
			// 外部通信制御：無効
			if (xctrl) {
				xctrl = false;
				console.info("external control is disable.");
			}
		}

		// 状態・システム
		if (bytes[2] & 0x80) {
			// 異常
			document.getElementById("state-mcu").style.color = 'red';
		} else {
			// 正常
			document.getElementById("state-mcu").style.color = 'green';
		}

		// 状態・出力
		if (bytes[2] & 0x20) {
			document.getElementById("state-out").style.color = 'red';
		} else if (2 == mode) {
			document.getElementById("state-out").style.color = 'green';
		} else {
			document.getElementById("state-out").style.color = 'currentColor';
		}
		
		// 入力電圧
		var volInput = bytes[3]*0.1;
		//console.info('volOut : ' + volOut);
		if (10 > volInput) {
			document.getElementById("volt-in").textContent = '!' + volInput.toFixed(1);
		} else {
			document.getElementById("volt-in").textContent = volInput.toFixed(1);
		}

		// 入力電圧
		var tempCpu = bytes[4] - 128;
		//console.info('volOut : ' + volOut);
		if (-10 >= tempCpu) {
			document.getElementById("temp-cpu").textContent = '-' + tempCpu;
		} else if (0 > tempCpu) {
			document.getElementById("temp-cpu").textContent = '!-' + tempCpu;
		} else if (10 > tempCpu) {
			document.getElementById("temp-cpu").textContent = '!!' + tempCpu;
		} else if (100 > tempCpu) {
			document.getElementById("temp-cpu").textContent = '!' + tempCpu;
		} else {
			document.getElementById("temp-cpu").textContent = tempCpu;
		}

	} else {
		console.warn("unknown data ... ", bytes);
	}
};
setCallbackMessage(parseSokeck);

const resetAliveTimer = () => {
	clearTimeout(aliveTimer);
	aliveTimer = setTimeout(() => {
		document.getElementById("out-fwd").style.fill = 'currentColor';
		document.getElementById("out-rvs").style.fill = 'currentColor';
		document.getElementById("state-mcu").style.color = 'currentColor';
		document.getElementById("state-out").style.color = 'currentColor';
		document.getElementById("volt-in").textContent = '--.-';
		document.getElementById("temp-cpu").textContent = '---';
	}, 1000);
};

function sendOutputCmd(duty) {
	var duty0, duty1;
	if (dir) {
		if (duty > 4095) {
			duty = 4095;
		}

		duty0 = duty & 0x00FF;
		duty1 = (duty & 0x3F00) >> 8;
		if (1 == dir) {
			// forward
			duty1 = duty1 + 64;
		} else if (2 == dir) {
			// reverse
			duty1 = duty1 + 128;
		}

		const ar_cmd = new Uint8Array(3);
		ar_cmd[0] = parseInt('12',16);
		ar_cmd[1] = duty0;
		ar_cmd[2] = duty1;

		sendWebSocketData(ar_cmd);
	}
}

function update_speed() {
	if (dir) {
		var duty = Math.floor(duty_slider.noUiSlider.get() * 4096 / 100);
		sendOutputCmd(duty);
	}
}

export const OutputOn = (direction) => {
	if (direction > 2) {
		dir = 0;
	} else {
		dir = direction;
		sendOutputCmd(0);
	}
};

export const OutputStop = () => {
	duty_slider.noUiSlider.set(0);
};

export const OutputOff = () => {
	dir = 0;
	duty_slider.noUiSlider.set(0);

	const ar_cmd = new Uint8Array(3);
	ar_cmd[0] = parseInt('12',16);
	ar_cmd[1] = parseInt('00',16);
	ar_cmd[2] = parseInt('00',16);

	sendWebSocketData(ar_cmd);
};
