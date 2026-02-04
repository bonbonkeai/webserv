#!/usr/bin/env php
<?php
echo "Content-Type: text/plain\r\n\r\n";

for ($i = 1; $i <= 5; $i++) {
    echo "chunk $i\n";
    flush();
    usleep(300000); // 300ms
}
