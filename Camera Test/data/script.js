// Variables globales
let currentSequenceIndex = 0;
let movements = [];

// Valores actuales de los controles
let currentStepperDist = 0;
let currentStepperSpeed = 50;
let currentServoAngle = 90;
let currentServoSpeed = 50;

function updateStatus() {
  fetch('/status')
    .then(response => response.json())
    .then(data => {
      const status = document.getElementById('status');
      const btn = document.getElementById('photoBtn');
      
      if(data.connected) {
        status.className = 'status connected';
        status.innerHTML = '🟢 Bluetooth Conectado';
        btn.disabled = false;
      } else {
        status.className = 'status disconnected';
        status.innerHTML = '🔴 Bluetooth Desconectado';
        btn.disabled = true;
      }
    })
    .catch(err => console.error('Error:', err));
}

function takePhoto() {
  const msg = document.getElementById('message');
  msg.textContent = '📸 Disparando foto...';
  msg.style.backgroundColor = '#e7f3ff';
  msg.style.color = '#667eea';
  
  fetch('/photo')
    .then(response => response.json())
    .then(data => {
      if(data.success) {
        msg.textContent = '✅ Foto tomada correctamente';
        msg.style.backgroundColor = '#d4edda';
        msg.style.color = '#155724';
      } else {
        msg.textContent = '❌ Error: ' + data.message;
        msg.style.backgroundColor = '#f8d7da';
        msg.style.color = '#721c24';
      }
      setTimeout(() => { 
        msg.textContent = ''; 
        msg.style.backgroundColor = 'transparent';
      }, 3000);
    })
    .catch(err => {
      msg.textContent = '❌ Error de conexión';
      msg.style.backgroundColor = '#f8d7da';
      msg.style.color = '#721c24';
      setTimeout(() => { 
        msg.textContent = ''; 
        msg.style.backgroundColor = 'transparent';
      }, 3000);
    });
}

// ========== Control Manual ==========

function updateStepperDist(value) {
  currentStepperDist = parseInt(value);
  document.getElementById('stepperDistValue').textContent = value + ' mm';
}

function updateStepperSpeed(value) {
  currentStepperSpeed = parseInt(value);
  document.getElementById('stepperSpeedValue').textContent = value + '%';
}

function updateServoAngle(value) {
  currentServoAngle = parseInt(value);
  document.getElementById('servoValue').textContent = value + '°';
}

function updateServoSpeed(value) {
  currentServoSpeed = parseInt(value);
  document.getElementById('servoSpeedValue').textContent = value + '%';
}

function moveStepperManual() {
  showMessage('🚂 Moviendo stepper...', 'info');
  fetch(`/stepper?distance=${currentStepperDist}&speed=${currentStepperSpeed}`)
    .then(response => response.json())
    .then(data => {
      if(data.success) {
        showMessage('✅ Stepper movido correctamente', 'success');
      } else {
        showMessage('❌ Error: ' + data.message, 'error');
      }
    })
    .catch(err => showMessage('❌ Error de conexión', 'error'));
}

function moveServoManual() {
  showMessage('📐 Moviendo servo...', 'info');
  fetch(`/servo?angle=${currentServoAngle}&speed=${currentServoSpeed}`)
    .then(response => response.json())
    .then(data => {
      if(data.success) {
        showMessage('✅ Servo movido correctamente', 'success');
      } else {
        showMessage('❌ Error: ' + data.message, 'error');
      }
    })
    .catch(err => showMessage('❌ Error de conexión', 'error'));
}

function zeroStepper() {
  showMessage('🔄 Reseteando posición...', 'info');
  fetch('/stepper/zero')
    .then(response => response.json())
    .then(data => {
      if(data.success) {
        showMessage('✅ Posición reseteada', 'success');
      } else {
        showMessage('❌ Error reseteando', 'error');
      }
    })
    .catch(err => showMessage('❌ Error de conexión', 'error'));
}

// ========== Programación de Secuencias ==========

function addMovement() {
  const distance = parseFloat(document.getElementById('seqDistance').value);
  const speed = parseInt(document.getElementById('seqSpeed').value);
  const angle = parseInt(document.getElementById('seqAngle').value);
  const angleSpeed = parseInt(document.getElementById('seqAngleSpeed').value);
  const pause = parseInt(document.getElementById('seqPause').value);
  const simultaneous = document.getElementById('seqSimul').checked;
  
  const movement = {
    distance: distance,
    speed: speed,
    angle: angle,
    angleSpeed: angleSpeed,
    pause: pause,
    simultaneous: simultaneous
  };
  
  movements.push(movement);
  updateMovementList();
  showMessage('✅ Movimiento agregado', 'success');
}

function updateMovementList() {
  const listDiv = document.getElementById('movementList');
  const countSpan = document.getElementById('movCount');
  
  countSpan.textContent = movements.length;
  
  if(movements.length === 0) {
    listDiv.innerHTML = '<p style="text-align:center;color:#999;">No hay movimientos</p>';
    return;
  }
  
  listDiv.innerHTML = '';
  movements.forEach((mov, index) => {
    const item = document.createElement('div');
    item.className = 'movement-item';
    item.innerHTML = `
      <div class="movement-info">
        <strong>#${index + 1}</strong> 
        ${mov.distance}mm @ ${mov.speed}% | 
        ${mov.angle}° @ ${mov.angleSpeed}%
        ${mov.simultaneous ? '<span class="badge">⚡ Simul.</span>' : ''}
        ${mov.pause > 0 ? `<span class="badge">⏸ ${mov.pause}ms</span>` : ''}
      </div>
    `;
    listDiv.appendChild(item);
  });
}

function clearSequence() {
  if(confirm('¿Limpiar toda la secuencia?')) {
    movements = [];
    updateMovementList();
    showMessage('🗑️ Secuencia limpiada', 'info');
  }
}

function executeSequence() {
  if(movements.length === 0) {
    showMessage('⚠️ No hay movimientos para ejecutar', 'error');
    return;
  }
  
  showMessage('🎬 Creando y ejecutando secuencia...', 'info');
  
  // Crear secuencia
  fetch('/sequence/create', {
    method: 'POST',
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body: 'name=TempSequence'
  })
  .then(response => response.json())
  .then(data => {
    if(!data.success) throw new Error('Error creando secuencia');
    
    currentSequenceIndex = data.index;
    
    // Agregar todos los movimientos
    const promises = movements.map(mov => {
      const params = new URLSearchParams({
        seq: currentSequenceIndex,
        distance: mov.distance,
        speed: mov.speed,
        angle: mov.angle,
        angleSpeed: mov.angleSpeed,
        simultaneous: mov.simultaneous,
        pause: mov.pause
      });
      
      return fetch('/sequence/add', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: params.toString()
      });
    });
    
    return Promise.all(promises);
  })
  .then(() => {
    // Ejecutar secuencia
    return fetch(`/sequence/execute?index=${currentSequenceIndex}`);
  })
  .then(response => response.json())
  .then(data => {
    if(data.success) {
      showMessage('▶️ Secuencia ejecutándose...', 'success');
    } else {
      showMessage('❌ Error ejecutando secuencia', 'error');
    }
  })
  .catch(err => {
    console.error(err);
    showMessage('❌ Error: ' + err.message, 'error');
  });
}

function showMessage(text, type) {
  const msg = document.getElementById('message');
  msg.textContent = text;
  
  switch(type) {
    case 'success':
      msg.style.backgroundColor = '#d4edda';
      msg.style.color = '#155724';
      break;
    case 'error':
      msg.style.backgroundColor = '#f8d7da';
      msg.style.color = '#721c24';
      break;
    case 'info':
      msg.style.backgroundColor = '#e7f3ff';
      msg.style.color = '#667eea';
      break;
  }
  
  setTimeout(() => {
    msg.textContent = '';
    msg.style.backgroundColor = 'transparent';
  }, 3000);
}

// Actualizar estado cada 2 segundos
setInterval(updateStatus, 2000);
updateStatus();