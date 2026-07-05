/**
 * ============================================================================
 * @file wifi.js
 * @brief УПРАВЛЕНИЕ WI-FI
 * @version 6.2
 * 
 * Содержит объект wifi для настройки сети через веб-интерфейс
 * ============================================================================
 */

// ============================================================================
// 9. УПРАВЛЕНИЕ WI-FI
// ============================================================================
const wifi = {
    init() {
        this.setupForm();
        // Запросим статус при загрузке (через WebSocket)
        setTimeout(() => {
            if (state.socket && state.socket.readyState === WebSocket.OPEN) {
                state.socket.send('WIFI_STATUS');
            }
        }, 1000);
    },
    
    setupForm() {
        const connectBtn = dom.get('wifiConnectBtn');
        const apBtn = dom.get('wifiAPBtn');
        const ssidInput = dom.get('wifiSSID');
        const passInput = dom.get('wifiPassword');
        
        if (connectBtn) {
            connectBtn.addEventListener('click', () => {
                const ssid = ssidInput.value.trim();
                const pass = passInput.value.trim();
                if (ssid.length === 0) {
                    alert('Введите SSID');
                    return;
                }
                if (state.socket && state.socket.readyState === WebSocket.OPEN) {
                    state.socket.send(`WIFI_SET:${ssid}:${pass}`);
                    connectBtn.textContent = 'Сохранение...';
                    connectBtn.disabled = true;
                }
            });
        }
        
        if (apBtn) {
            apBtn.addEventListener('click', () => {
                if (confirm('Переключиться в режим AP (точка доступа)?')) {
                    if (state.socket && state.socket.readyState === WebSocket.OPEN) {
                        state.socket.send('WIFI_AP');
                        apBtn.textContent = 'Переключение...';
                        apBtn.disabled = true;
                    }
                }
            });
        }
    }
};