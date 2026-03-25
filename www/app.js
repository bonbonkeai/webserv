const metaEl = document.getElementById('meta');
const headersEl = document.getElementById('headers');
const bodyEl = document.getElementById('body');
const previewEl = document.getElementById('preview');

function escapeHtml(str) {
  return str
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;');
}

function clearPreview() {
  previewEl.innerHTML = 'No preview yet.';
}

function renderPreview(contentType, text, url) {
  previewEl.innerHTML = '';

  if (!contentType) {
    previewEl.innerHTML = `<p class="preview-note">No Content-Type returned.</p>`;
    return;
  }

  if (contentType.includes('text/html')) {
    const iframe = document.createElement('iframe');
    iframe.setAttribute('sandbox', 'allow-same-origin');
    iframe.srcdoc = text;
    previewEl.appendChild(iframe);
    return;
  }

  if (contentType.startsWith('text/plain') || contentType.includes('application/json')) {
    const pre = document.createElement('pre');
    pre.textContent = text;
    pre.style.margin = '0';
    pre.style.whiteSpace = 'pre-wrap';
    pre.style.wordBreak = 'break-word';
    previewEl.appendChild(pre);
    return;
  }

  if (contentType.startsWith('image/')) {
    const img = document.createElement('img');
    img.src = url + (url.includes('?') ? '&' : '?') + '_ts=' + Date.now();
    img.alt = 'Image preview';
    previewEl.appendChild(img);
    return;
  }

  previewEl.innerHTML = `<p class="preview-note">Preview not available for Content-Type: <code>${escapeHtml(contentType)}</code></p>`;
}

async function sendRequest(method, path) {
  const errorLinkBoxEl = document.getElementById('errorLinkBox');

  metaEl.textContent = `Loading ${method} ${path} ...`;
  headersEl.textContent = '';
  bodyEl.textContent = '';
  clearPreview();
  errorLinkBoxEl.innerHTML = '';

  try {
    const response = await fetch(path, {
      method,
      redirect: 'follow'
    });

    const contentType = response.headers.get('content-type') || '';
    const text = await response.text();

    metaEl.textContent = `${method} ${path} → ${response.status} ${response.statusText}`;

    let headersText = '';
    response.headers.forEach((value, key) => {
      headersText += `${key}: ${value}\n`;
    });
    headersEl.textContent = headersText || '(no headers)';

    bodyEl.textContent = text || '(empty body)';

    const errorPageMap = {
      400: '/html_error/400.html',
      403: '/html_error/403.html',
      404: '/html_error/404.html',
      405: '/html_error/405.html',
      408: '/html_error/408.html',
      411: '/html_error/411.html',
      413: '/html_error/413.html',
      414: '/html_error/414.html',
      500: '/html_error/500.html',
      501: '/html_error/501.html',
      502: '/html_error/502.html',
      504: '/html_error/504.html'
    };

    const errorPage = errorPageMap[response.status];
    if (errorPage) {
      errorLinkBoxEl.innerHTML = `
        <div class="error-link-card">
          Open configured error page:
          <a href="${errorPage}" target="_blank" rel="noopener noreferrer">
            ${errorPage}
          </a>
        </div>
      `;
    }

    renderPreview(contentType, text, path);
  } catch (err) {
    metaEl.textContent = `${method} ${path} → request failed`;
    headersEl.textContent = '';
    bodyEl.textContent = String(err);
    previewEl.innerHTML = `<p class="preview-note">Request failed.</p>`;
    errorLinkBoxEl.innerHTML = '';
  }
}

async function sendPostText() {
  const path = '/cgi-bin/echo_body.sh';
  const payload = 'hello=world&from=tester';

  metaEl.textContent = `Loading POST ${path} ...`;
  headersEl.textContent = '';
  bodyEl.textContent = '';
  clearPreview();

  try {
    const response = await fetch(path, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded'
      },
      body: payload
    });

    const contentType = response.headers.get('content-type') || '';
    const text = await response.text();

    metaEl.textContent = `POST ${path} → ${response.status} ${response.statusText}`;

    let headersText = '';
    response.headers.forEach((value, key) => {
      headersText += `${key}: ${value}\n`;
    });
    headersEl.textContent = headersText || '(no headers)';

    bodyEl.textContent = text || '(empty body)';

    renderPreview(contentType, text, path);
  } catch (err) {
    metaEl.textContent = `POST ${path} → request failed`;
    headersEl.textContent = '';
    bodyEl.textContent = String(err);
    previewEl.innerHTML = `<p class="preview-note">Request failed.</p>`;
  }
}

async function uploadFile() {
  const input = document.getElementById('fileInput');
  if (!input.files || !input.files[0]) {
    metaEl.textContent = 'Please choose a file first.';
    return;
  }

  const formData = new FormData();
  formData.append('file', input.files[0]);

  metaEl.textContent = `Uploading ${input.files[0].name} ...`;
  headersEl.textContent = '';
  bodyEl.textContent = '';
  clearPreview();

  try {
    const response = await fetch('/upload', {
      method: 'POST',
      body: formData
    });

    const contentType = response.headers.get('content-type') || '';
    const text = await response.text();

    metaEl.textContent = `POST /upload → ${response.status} ${response.statusText}`;

    let headersText = '';
    response.headers.forEach((value, key) => {
      headersText += `${key}: ${value}\n`;
    });
    headersEl.textContent = headersText || '(no headers)';

    bodyEl.textContent = text || '(empty body)';

    renderPreview(contentType, text, '/upload');
  } catch (err) {
    metaEl.textContent = 'Upload failed';
    headersEl.textContent = '';
    bodyEl.textContent = String(err);
    previewEl.innerHTML = `<p class="preview-note">Upload failed.</p>`;
  }
}

async function deleteFile() {
  const input = document.getElementById('deleteName');
  const name = input.value.trim();

  if (!name) {
    metaEl.textContent = 'Please enter a file name to delete.';
    return;
  }

  const path = `/upload/${encodeURIComponent(name)}`;

  metaEl.textContent = `Deleting ${path} ...`;
  headersEl.textContent = '';
  bodyEl.textContent = '';
  clearPreview();

  try {
    const response = await fetch(path, {
      method: 'DELETE'
    });

    const contentType = response.headers.get('content-type') || '';
    const text = await response.text();

    metaEl.textContent = `DELETE ${path} → ${response.status} ${response.statusText}`;

    let headersText = '';
    response.headers.forEach((value, key) => {
      headersText += `${key}: ${value}\n`;
    });
    headersEl.textContent = headersText || '(no headers)';

    bodyEl.textContent = text || '(empty body)';

    renderPreview(contentType, text, path);
  } catch (err) {
    metaEl.textContent = `DELETE ${path} → request failed`;
    headersEl.textContent = '';
    bodyEl.textContent = String(err);
    previewEl.innerHTML = `<p class="preview-note">Delete failed.</p>`;
  }
}

function triggerMissingLength411() {
  const metaEl = document.getElementById('meta');
  const headersEl = document.getElementById('headers');
  const bodyEl = document.getElementById('body');
  const previewEl = document.getElementById('preview');
  const errorLinkBoxEl = document.getElementById('errorLinkBox');

  metaEl.textContent = '411 test note';
  headersEl.textContent = '';
  bodyEl.textContent =
    'This case is best tested with nc or curl because browsers automatically manage request framing.\n\n' +
    'Example:\n' +
    "printf 'POST /upload/test.txt HTTP/1.1\\r\\nHost: localhost\\r\\n\\r\\nhello' | nc 127.0.0.1 8080\n\n" +
    'Expected result: HTTP/1.1 411 Length Required';
  previewEl.innerHTML = '<p class="preview-note">Use terminal for this parser-level test.</p>';
  errorLinkBoxEl.innerHTML = `
    <div class="error-link-card">
      Open configured error page:
      <a href="/html_error/411.html" target="_blank" rel="noopener noreferrer">
        /html_error/411.html
      </a>
    </div>
  `;
}

async function trigger413OversizedUpload() {
  const metaEl = document.getElementById('meta');
  const headersEl = document.getElementById('headers');
  const bodyEl = document.getElementById('body');
  const previewEl = document.getElementById('preview');
  const errorLinkBoxEl = document.getElementById('errorLinkBox');

  metaEl.textContent = 'Loading oversized upload test...';
  headersEl.textContent = '';
  bodyEl.textContent = '';
  previewEl.innerHTML = 'No preview yet.';
  errorLinkBoxEl.innerHTML = '';

  try {
    const big = 'A'.repeat(600 * 1024); // 600 KB, above /upload/ 500K
    const response = await fetch('/upload/oversized_from_browser.txt', {
      method: 'POST',
      headers: {
        'Content-Type': 'text/plain'
      },
      body: big
    });

    const text = await response.text();
    const contentType = response.headers.get('content-type') || '';

    metaEl.textContent = `POST /upload/oversized_from_browser.txt → ${response.status} ${response.statusText}`;

    let headersText = '';
    response.headers.forEach((value, key) => {
      headersText += `${key}: ${value}\n`;
    });
    headersEl.textContent = headersText || '(no headers)';
    bodyEl.textContent = text || '(empty body)';

    if (response.status === 413) {
      errorLinkBoxEl.innerHTML = `
        <div class="error-link-card">
          Open configured error page:
          <a href="/html_error/413.html" target="_blank" rel="noopener noreferrer">
            /html_error/413.html
          </a>
        </div>
      `;
    }

    if (contentType.includes('text/html')) {
      previewEl.innerHTML = `<iframe sandbox="allow-same-origin" srcdoc="${text.replace(/"/g, '&quot;')}"></iframe>`;
    } else {
      previewEl.innerHTML = '<p class="preview-note">Oversized upload test completed.</p>';
    }
  } catch (err) {
    metaEl.textContent = 'Oversized upload test failed';
    bodyEl.textContent = String(err);
    previewEl.innerHTML = '<p class="preview-note">Request failed.</p>';
  }
}
function copySiegeCommand() {
  const cmd = document.getElementById('siegeCommand').textContent;
  navigator.clipboard.writeText(cmd).then(() => {
    const metaEl = document.getElementById('meta');
    if (metaEl) {
      metaEl.textContent = 'Siege command copied to clipboard.';
    }
  }).catch(() => {
    const metaEl = document.getElementById('meta');
    if (metaEl) {
      metaEl.textContent = 'Could not copy automatically. Please copy manually.';
    }
  });
}

function renderSiegeOutput() {
  const input = document.getElementById('siegeOutput');
  const result = document.getElementById('siegeResult');
  const text = input.value.trim();

  if (!text) {
    result.textContent = 'No siege result yet.';
    return;
  }

  result.textContent = text;
}

function clearSiegeOutput() {
  document.getElementById('siegeOutput').value = '';
  document.getElementById('siegeResult').textContent = 'No siege result yet.';
}

function clearErrorLink() {
  const errorLinkBoxEl = document.getElementById('errorLinkBox');
  if (errorLinkBoxEl) {
    errorLinkBoxEl.innerHTML = '';
  }
}

function renderErrorLink(status) {
  const errorLinkBoxEl = document.getElementById('errorLinkBox');
  if (!errorLinkBoxEl) return;

  const errorPageMap = {
    400: '/html_error/400.html',
    403: '/html_error/403.html',
    404: '/html_error/404.html',
    405: '/html_error/405.html',
    408: '/html_error/408.html',
    411: '/html_error/411.html',
    413: '/html_error/413.html',
    414: '/html_error/414.html',
    500: '/html_error/500.html',
    501: '/html_error/501.html',
    502: '/html_error/502.html',
    504: '/html_error/504.html'
  };

  const errorPage = errorPageMap[status];
  if (!errorPage) {
    errorLinkBoxEl.innerHTML = '';
    return;
  }

  errorLinkBoxEl.innerHTML = `
    <div class="error-link-card">
      Open configured error page:
      <a href="${errorPage}" target="_blank" rel="noopener noreferrer">
        ${errorPage}
      </a>
    </div>
  `;
}