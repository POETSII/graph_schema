<?php
$file = "log.csv";
echo("Attempting to delete the log!");
if (!unlink($file)) {
	echo ("Error deleting $file");
}
?>
