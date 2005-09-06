<?php
	include('./header.php');
	include('./menu.php');
?>

<p>
This part of GNOME Power Manager has a test suite, gnome-power-dbus-test that can be used to test the out-of-order and error conditions.
It also shows any limitations or bugs in GNOME Power Manager.
For example, running gnome-power-dbus-test --doNACK and selecting shutdown from the drop down menu would give the following dialog from GPM:
</p>

<center>
<img src="images/nack-sunday.png" alt="[img]"/>
</center>
<p class="caption">
The error message from the GNOME Power Manager 0.1.0
</p>

<center>
<img src="images/nack-libnotify.png" alt="[img]"/>
<img src="images/nothing-libnotify.png" alt="[img]"/>
</center>
<p class="caption">
The libnotify error messages using CVS.
</p>

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

<h3>Server Signals</h3>
<p>
The signals are emitted when state is changed or an action is about to happen.
</p>

<h3>mainsStatusChanged (bool isRunningOnMains)</h3>
<p>emitted when the mains power connection changes</p>
<pre>status: complete</pre>

<h3>userIdleStatusChanged (bool isIdle)</h3>
<p>emitted when the use is idle / no longer idle</p>
<pre>status: none</pre>

<h3>actionAboutToHappen (enum action)</h3>
<p>registered vetoers MUST respond to this signal using Ack, vetoNAK, vetoWait 
with ten seconds otherwise they are kicked out as vetoers (to work around 
broken apps)</p>
<pre>status: complete</pre>

<h3>performingAction (enum action)</h3>
<p>Just a signal, no response is required, nor allowed.</p>
<pre>status: complete</pre>

<p>
Action is defined as: 
</p>

<pre>
SCREENSAVE =  1, 
POWEROFF   =  2, 
SUSPEND    =  4, 
HIBERNATE  =  8, 
LOGOFF     =  16, 
ALL        =  255
</pre>

<h2>Client Methods</h2>

<p>
Client signals are sent from an application to GPM.
If an application registers its interest in a action, it is given the chance to ack, or nack the situation.
</p>
<p>
The situation:
User presses shutdown, but Abiword has an unsaved document open.
Abiword is informed of the impending shutdown, but issues a nack, followed by a screen that asks the user if he would like to save the document.
It can then resume the shutdown using the signal ack.
</p>
<p>
Similarly, if Evolution is indexing a database (that would be corrupted if interrupted) and needs at least 3 seconds to flush buffers, it can pause the shutdown using wait.
The signals currently available are:
</p>

<h3>bool ActionRegister (enum action, gchar localizedAppname)</h3>
<p>Used for signing up for vetoing a specific action</p>
<pre>status: complete</pre>

<h3>bool ActionUnregister (enum action)</h3>
<p>Used to unregister the applications interest in a specific action.
Note: GPM will automatically unregister clients that disconnect from the bus</p>
<pre>status: semi-complete, assumes ALL</pre>

<h3>bool Ack (enum action)</h3>
Vetoer allows a specific action to take place
<pre>status: complete</pre>

<h3>bool Nack (enum action, gchar localizedReason)</h3>
<p>Vetoer doesn't allow a specific action to take place; g-p-m SHOULD display a 
notification to the user that the action cannot take place and what the vetoing 
application is.</p>
<pre>status: complete</pre>

<h3>bool Wait (enum action, int timeout, gchar localizedReason)</h3>
<p>Vetoer asks for a timeout of timeout ms before g-p-m should try to perform the 
action again. It is legal for the vetoer to call Ack before the timeout.
If the (accumulated) delay amounts to more than one second, g-p-m SHOULD 
display a notification about who is blocking the action.</p>
<pre>status: not present yet</pre>

<?php
	include('footer.php');
?>
