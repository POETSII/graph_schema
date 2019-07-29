#!/bin/bash

ffmpeg -r 15 -i out%04d.png -vcodec libx264 -crf 25  test.mp4
