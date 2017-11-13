<?php
$file = "/vagrant/apps/firefly_sync/fireflyGraph/_tmp/log.csv";
echo("Attempting to delete the log!");
if (!unlink($file)) {
	echo ("Error deleting $file");
}
?>
