<h1>KryonOS Web Browser Test App</h1>
<p>This directory contains the test application files for the KryonOS web browser, along with a dedicated backend proxy script.</p>

<hr>

<h2>File Structure</h2>
<ul>
  <li><code>app.json</code> &mdash; Application manifest and configuration for KryonOS.</li>
  <li><code>main.js</code> &mdash; Frontend JavaScript application logic executed on the KryonOS device.</li>
  <li><code>proxy.php</code> &mdash; Backend web proxy engine (run on an external server, <b>not</b> on the ESP32 device).</li>
</ul>

<hr>

<h2>Setup &amp; Usage Instructions</h2>

<h3>1. Run the Backend Proxy (External Server)</h3>
<p>The <code>proxy.php</code> script handles web scraping and page preprocessing. Do <b>not</b> install or upload <code>proxy.php</code> to your KryonOS device. Run it separately on a machine with PHP installed:</p>
<ol>
  <li>Place <code>proxy.php</code> on your local computer or web server.</li>
  <li>Start a local PHP server in that directory:
    <pre><code>php -S 0.0.0.0:8000</code></pre>
  </li>
  <li>Determine your host machine's local IP address (e.g., <code>192.168.1.50</code>).</li>
  <li>Your complete proxy endpoint URL will be:
    <pre><code>http://&lt;your-server-ip&gt;:8000/proxy.php</code></pre>
  </li>
</ol>

<h3>2. Install the Browser App on KryonOS</h3>
<ol>
  <li>Upload only <code>app.json</code> and <code>main.js</code> to your KryonOS device under your application directory.</li>
  <li>Launch the Browser app from the KryonOS application menu.</li>
  <li>When prompted (or within the browser configuration), enter your full proxy endpoint URL (e.g., <code>http://192.168.1.50:8000/proxy.php</code>).</li>
</ol>

<hr>

<blockquote>
  <b>Notice:</b> This application and proxy script were developed for internal testing and validation. They may contain bugs or display rendering limitations on complex web pages.
</blockquote>
