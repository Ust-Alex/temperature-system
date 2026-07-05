/**
 * ============================================================================
 * @file state.js
 * @brief СОСТОЯНИЕ ВЕБ-ИНТЕРФЕЙСА
 * @version 6.2
 * 
 * Содержит глобальное состояние (state), флаги и вспомогательные объекты
 * ============================================================================
 */

// ============================================================================
// 2. СОСТОЯНИЕ
// ============================================================================
const state = {
    socket: null,
    reconnectAttempts: 0,
    reconnectTimeout: null,
    watchdogTimer: null,
    reconnectStopped: false,
    lastDataTime: Date.now(),
    pageVisible: true,
    pendingData: null,
    chartUpdateTimer: null,
    dataBuffer: {
        time: new Array(CONFIG.MAX_POINTS).fill(''),
        temps: [
            new Array(CONFIG.MAX_POINTS).fill(null),
            new Array(CONFIG.MAX_POINTS).fill(null),
            new Array(CONFIG.MAX_POINTS).fill(null),
            new Array(CONFIG.MAX_POINTS).fill(null),
            new Array(CONFIG.MAX_POINTS).fill(null),
            new Array(CONFIG.MAX_POINTS).fill(null)
        ],
        index: 0,
        count: 0,
        lastTime: ''
    },
    lastValues: {
        mode: -1,
        color: -1,
        time: '',
        baseTemp: null,
        temps: [null, null, null, null, null, null]
    },
    currentRange: 60,
    chart: null,
    wifiStatus: { mode: '--', ip: '--' }
};

// ============================================================================
// 3. ЗВУКОВОЕ СОПРОВОЖДЕНИЕ (объект, но инициализация в sound.js)
// ============================================================================
const soundManager = {
    tormazi: new Audio('/tormazi.wav'),
    zhdati: new Audio('/zhdati.wav'),
    interval: null,
    yellowCycleState: false,
    audioEnabled: false,
    audioContext: null,
    
    init() {
        console.log('[ЗВУК] Инициализация...');
        try {
            const AudioCtor = window.AudioContext || window.webkitAudioContext;
            if (AudioCtor) {
                this.audioContext = new AudioCtor();
            }
        } catch(e) {}
        
        const unlock = () => {
            if (this.audioContext && this.audioContext.state === 'suspended') {
                this.audioContext.resume();
            }
            this.audioEnabled = true;
            document.removeEventListener('touchstart', unlock);
            document.removeEventListener('touchend', unlock);
            document.removeEventListener('click', unlock);
            document.removeEventListener('keydown', unlock);
        };
        
        document.addEventListener('touchstart', unlock, { once: true });
        document.addEventListener('touchend', unlock, { once: true });
        document.addEventListener('click', unlock, { once: true });
        document.addEventListener('keydown', unlock, { once: true });
    },
    
    play(sound) {
        if (!sound || !this.audioEnabled) return;
        sound.currentTime = 0;
        sound.play().catch(e => {});
    },
    
    stopAll() {
        if (this.interval) {
            clearInterval(this.interval);
            this.interval = null;
        }
        this.yellowCycleState = false;
    },
    
    startYellowCycle() {
        this.stopAll();
        this.yellowCycleState = false;
        this.interval = setInterval(() => {
            this.play(this.yellowCycleState ? this.tormazi : this.zhdati);
            this.yellowCycleState = !this.yellowCycleState;
        }, 60000);
    },
    
    startRedCycle() {
        this.stopAll();
        this.interval = setInterval(() => {
            this.play(this.tormazi);
        }, 30000);
    }
};