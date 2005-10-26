<?php
	include('./header.php');
	include('./menu.php');
?>

<h2>Client Methods</h2>

<h3>bool isUserIdle ()</h3>
<p>Returns true is the user has been idle for the timeout set in gconf.</p>
<pre>status: stub</pre>

<h3>bool isOnBattery ()</h3>
<p>Returns true if we are running on batteries</p>
<pre>status: complete</pre>

<h3>bool isOnUps ()</h3>
<p>Returns true if we are running on UPS Power</p>
<pre>status: complete</pre>

<h3>bool isOnAc ()</h3>
<p>Returns true if we are running on AC (i.e. plugged in power source)</p>
<pre>status: complete</pre>

<h2>Server Signals</h2>
<p>
The signals are emitted when state is changed or an action is about to happen.
</p>

<h3>mainsStatusChanged (bool isRunningOnMains)</h3>
<p>emitted when the mains power connection changes</p>

<?php
	include('footer.php');
?>
