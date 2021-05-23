#include <cstdint>
#include <vector>
#include <queue>
#include <tuple>

struct event_t
{
    uint32_t t;
    uint8_t x, y;
    int8_t delta;
    uint8_t abs;
};


std::vector<event_t> reconstruct_events(
    unsigned w, unsigned h,
    const std::vector<event_t> &changes,
    std::vector<uint8_t> &working
)
{
    for(const auto & c : changes){
        working[c.y*w+c.x]=c.abs;
    }
}


int main(int argc, char *argv[])
{
    int w=atoi(argv[1]);
    int h=atoi(argv[2]);

    std::vector<uint8_t> current(w*h,127);
    
    std::vector<event_t> changes;

    unsigned t=0;
    while(1){
        uint32_t count;
        int got;
        got=fread(&count, 4, 1, stdin);
        if(got==0){
            return 0;
        }else if(got!=4){
            return 1;
        }

        changes.resize(count);

        if(changes.size() != fread(&changes[0], sizeof(event_t), count, stdin)){
            return 1;
        }

        reconstruct_events(w,h, changes, current);

        if(w*h != fwrite(&current[0], 1, w*h, stdout)){
            return 1;
        }

        t++;
    }
}
