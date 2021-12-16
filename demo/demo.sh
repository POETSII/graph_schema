export GRAPH_SCHEMA_DIR=/mnt/c/UserData/dt10/external/POETS/graph_schema
export POETS_PROVIDER_PATH=${GRAPH_SCHEMA_DIR}/providers
export PYTHONPATH=${GRAPH_SCHEMA_DIR}/tools

# Create a graph instance
../apps/clock_tree/create_clock_tree_instance.py 4

# Save to file
../apps/clock_tree/create_clock_tree_instance.py 4 > clock.xml

# Print main graph properties
../tools/print_graph_properties.py clock.xml

# Alternative parser (sanity check)
../bin/print_graph_properties clock.xml

# Simulate using epoch_sim
../bin/epoch_sim clock.xml

# Generating snapshots
 ../bin/epoch_sim --help
../bin/epoch_sim clock.xml --snapshots 1 snapshots.xml
less snapshots.xml

# Visualising snapshots
mkdir tmp

# Turn them into dot
../tools/render_graph_as_dot.py clock.xml --snapshots snapshots.xml --output tmp \
    --bind-dev "branch" "state" "status" "color" "'gray' if value==0 else 'green' if value==1 else 'red'" \
    --bind-dev "leaf" "rts" "-" "color" "'red' if value>0 else 'green'"

# Turn them into png
for i in *.dot ; do neato $i -Tpng -O ; done

# Turn them into gif
ffmpeg -y -r 10 -i tmp_%06d.dot.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -crf 18 clock.mp4

# Clean up
rm *.dot *.dot.png *.svg


# Get event log
../bin/epoch_sim --help
../bin/epoch_sim clock.xml --log-events events.xml
less events.xml

# Limit the time
../bin/epoch_sim --help
../bin/epoch_sim clock.xml --log-events events.xml --max-steps 10

# Render it as a graph
../tools/render_event_log_as_dot.py events.xml --output events.dot
less events.dot

# Convert it to svg
 dot -Tsvg events.dot -O


# Graph sim for out of order
 ../bin/graph_sim clock.xml

# Various strategies
../bin/graph_sim --help

../bin/graph_sim clock.xml --strategy FIFO
../bin/graph_sim clock.xml --strategy Random
../bin/graph_sim clock.xml --strategy LIFO

../bin/graph_sim clock.xml --strategy LIFO --max-events 100 --log-events events.xml

