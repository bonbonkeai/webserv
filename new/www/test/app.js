function showResult(method, url, status, statusText, headers, body, duration) {
  document.getElementById('meta').textContent =
    `${method} ${url}\nstatus: ${status} ${statusText}\ntime: ${duration} ms`;

  let headerText = '';
  headers.forEach((value, key) => {
    headerText += `${key}: ${value}\n`;
  });
  document.getElementById('headers').textContent = headerText;
  document.getElementById('body').textContent = body;
}

async function sendRequest(method, url, options = {}) {
  const start = performance.now();
  try {
    const response = await fetch(url, {
      method: method,
      redirect: 'follow',
      ...options
    });

    const duration = Math.round(performance.now() - start);
    const body = await response.text();
    showResult(method, url, response.status, response.statusText, response.headers, body, duration);
  } catch (err) {
    document.getElementById('meta').textContent = `${method} ${url}\nrequest failed`;
    document.getElementById('headers').textContent = '';
    document.getElementById('body').textContent = String(err);
  }
}

function sendPostText() {
  const body = 'name=bonbon&project=webserv';
  sendRequest('POST', '/cgi-bin/echo_body.sh', {
    headers: {
      'Content-Type': 'application/x-www-form-urlencoded'
    },
    body: body
  });
}

async function uploadFile() {
  const input = document.getElementById('fileInput');
  if (!input.files.length) {
    alert('Choose a file first');
    return;
  }

  const form = new FormData();
  form.append('file', input.files[0]);

  await sendRequest('POST', '/upload', {
    body: form
  });
}

function deleteFile() {
  const name = document.getElementById('deleteName').value.trim();
  if (!name) {
    alert('Enter a filename');
    return;
  }
  sendRequest('DELETE', '/upload/' + encodeURIComponent(name));
}