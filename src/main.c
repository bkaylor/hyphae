#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include "SDL.h"
#include "SDL_ttf.h"
#include "SDL_image.h"

#include "vec2.h"
#include "draw.h"

typedef struct {
    int x;
    int y;
} Window;

typedef struct {
    Circle circle;
    bool has_spawned;

    float direction;
    float jitter;
    float spacing;
    SDL_Color color;

    int branch;
    int id;
} Node;

typedef struct {
    TTF_Font *font;
    SDL_Color font_color;
    bool show;
    bool do_iteration;
    bool quit;
    bool render_intermediate;
    Uint64 frames;
} UI;

#define NODES_ON_HEAP
// #undef NODES_ON_HEAP

#ifdef NODES_ON_HEAP
#define MAX_NODES 40000
#else
#define MAX_NODES 4000
#endif

typedef struct {
#ifdef NODES_ON_HEAP
    Node *nodes;
#else
    Node nodes[MAX_NODES];
#endif
    int node_count;
    int initial_point_count;

    int active_point_count;

    int max_nodes;

    int next_branch;
    int next_id;

    Window window;
} Graph;

void render(SDL_Renderer *renderer, Graph graph, UI ui)
{
    SDL_RenderClear(renderer);

    // Set background color.
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, NULL);

    if (ui.render_intermediate)
    {
        // Draw nodes.
        for (int i = 0; i < graph.node_count; i += 1)
        {
            Node node = graph.nodes[i];
            draw_circle(renderer, node.circle, node.color);
        }
    }

    if (ui.show)
    {
        // Draw UI.
        char initial_points_string[20];
        sprintf(initial_points_string, "%d initial points (up/down to change)", graph.initial_point_count);
        draw_text(renderer, 5, 5 + 12*0, initial_points_string, ui.font, ui.font_color);

        char active_points_string[20];
        sprintf(active_points_string, "%d/%d points (%d active)", graph.node_count, graph.max_nodes, graph.active_point_count);
        draw_text(renderer, 5, 5 + 12*1, active_points_string, ui.font, ui.font_color);

        char fps_string[20];
        sprintf(fps_string, "%I64d fps", ui.frames);
        draw_text(renderer, 5, 5 + 12*2, fps_string, ui.font, ui.font_color);

        char rendering_option_string[20];
        sprintf(rendering_option_string, "intermediate rendering %s (f to toggle)", ui.render_intermediate ? "on" : "off");
        draw_text(renderer, 5, 5 + 12*3, rendering_option_string, ui.font, ui.font_color);

        char show_string[20];
        sprintf(show_string, "(tab to show/hide)");
        draw_text(renderer, 5, 5 + 12*4, show_string, ui.font, ui.font_color);
    }

    SDL_RenderPresent(renderer);
}

void update(UI *ui, Graph *graph) 
{
    bool starting_new = false;

    if (ui->do_iteration)
    {
        // Zero out the graph.
        graph->node_count = 0;
        graph->next_branch = 1;
        graph->next_id = 0;

        float initial_spacing = 14;
        float initial_jitter = 0.002;
        float initial_radius = 14;

        // TODO(bkaylor): Initial nodes' directions should always be away from the center(?)

        for (int i = 0; i < graph->initial_point_count; i += 1)
        {
            Node initial_node;
            initial_node.circle = (Circle){{((i+1) * graph->window.x/(graph->initial_point_count+1)), graph->window.y/2}, initial_radius};
            initial_node.has_spawned = false;
            initial_node.direction = (float)(rand() % 360);
            initial_node.branch = graph->next_branch;
            initial_node.jitter = initial_jitter;
            initial_node.spacing = initial_spacing;
            initial_node.color = (SDL_Color){(rand()%200) + 56, (rand()%200) + 56, (rand()%200) + 56, 255};

            graph->next_branch += 1;

            graph->nodes[graph->node_count] = initial_node;
            graph->node_count += 1;
        }

        ui->do_iteration = false;
        starting_new = true;
    }

    if (!ui->render_intermediate && graph->active_point_count < 1)
    {
        ui->render_intermediate = true;
    }
    if (!starting_new && graph->active_point_count < 1) return; 

#ifdef NODES_ON_HEAP
    Node *nodes_to_add = malloc(sizeof(Node) * MAX_NODES);
#else
    Node nodes_to_add[MAX_NODES];
#endif
    int nodes_to_add_count = 0;

    if (graph->node_count < graph->max_nodes)
    {
        for (int i = 0; i < graph->node_count; i += 1)
        {
            Node *node = &graph->nodes[i];
            if (node->has_spawned) continue;

            // Continue the branch.
            bool heads = rand() % 2 == 0;

            Node new_node;
            new_node.has_spawned = false;
            new_node.branch = node->branch;

            new_node.id = graph->next_id;
            graph->next_id += 1;

            new_node.direction = node->direction + (heads ? node->jitter : (node->jitter * -1));
            new_node.jitter = node->jitter;
            new_node.spacing = node->spacing;
            new_node.circle = (Circle){
                vec2_add(node->circle.center, vec2_scalar_multiply(vec2_normalize(vec2_rotate((vec2){1, 0}, new_node.direction)), new_node.spacing)),
                node->circle.radius
            };
            new_node.color = node->color;

            node->has_spawned = true;

            // Only add the node if it doesn't collide with another branch.
            bool new_node_collided = false;
            for (int j = 0; j < graph->node_count; j += 1)
            {
                // We don't care about collisions with our own branch.
                if (new_node.branch == graph->nodes[j].branch) continue;

                // We don't care about collisions with circles spawned close to us.
                if (abs(new_node.id - graph->nodes[j].id) < 50) continue;

                // Make sure this circle didn't go off the screen.
                if (new_node.circle.center.x < 0 || new_node.circle.center.y < 0 || 
                    new_node.circle.center.x > graph->window.x || new_node.circle.center.y > graph->window.y)
                {
                    new_node_collided = true;
                    break;
                }

                if (do_circles_collide(new_node.circle, graph->nodes[j].circle))
                {
                    new_node_collided = true;
                    break;
                }
            }

            if (new_node_collided) continue;

            nodes_to_add[nodes_to_add_count] = new_node;
            nodes_to_add_count += 1;

            // Additionally, give each new node a small chance to start a new smaller branch.
            if (new_node.circle.radius > 2 && rand() % 100 < ((new_node.branch+1.5)*1.5))
            {
                bool heads = rand() % 2 == 0;

                Node new_branch;
                new_branch.has_spawned = false;

                new_branch.branch = graph->next_branch;
                graph->next_branch += 1;

                new_branch.id= graph->next_id;
                graph->next_id += 1;

                float radius_shrink_factor = 1.5;
                float jitter_growth_factor = 1.2;

                new_branch.jitter = new_node.jitter * jitter_growth_factor;
                new_branch.spacing = new_node.spacing / radius_shrink_factor;
                new_branch.direction = new_node.direction + (heads ? 1.5 : -1.5);
                new_branch.circle = (Circle){
                    vec2_add(new_node.circle.center, vec2_scalar_multiply(vec2_normalize(vec2_rotate((vec2){1, 0}, new_branch.direction)), new_branch.spacing*2.5)),
                    new_node.circle.radius/radius_shrink_factor
                };

                float color_darken_factor = 0.8;
                new_branch.color = (SDL_Color){new_node.color.r*color_darken_factor, new_node.color.g*color_darken_factor, new_node.color.b*color_darken_factor, new_node.color.a};

                nodes_to_add[nodes_to_add_count] = new_branch;
                nodes_to_add_count += 1;
            }
        }

        graph->active_point_count = nodes_to_add_count;
        for (int i = 0; i < nodes_to_add_count; i += 1)
        {
            graph->nodes[graph->node_count] = nodes_to_add[i];
            graph->node_count += 1;
        }
    }

    return;
}

void get_input(UI *ui, Graph *graph, SDL_Renderer *ren)
{
    // Handle events.
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                    case SDLK_ESCAPE:
                        ui->quit = true;
                        break;

                    case SDLK_SPACE:
                        ui->do_iteration = true;
                        break;

                    case SDLK_TAB:
                        ui->show = !ui->show;
                        break;
                    
                    case SDLK_f:
                        ui->render_intermediate= !ui->render_intermediate;
                        break;

                    case SDLK_UP:
                        graph->initial_point_count += 1;
                        break;

                    case SDLK_DOWN:
                        if (graph->initial_point_count > 0)
                        {
                            graph->initial_point_count -= 1;
                        }
                        break;

                    default:
                        break;
                }
                break;

            case SDL_QUIT:
                ui->quit = true;
                break;

            default:
                break;
        }
    }
}

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_EVERYTHING);
    IMG_Init(IMG_INIT_PNG);

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("SDL_Init video error: %s\n", SDL_GetError());
        return 1;
    }

    if (SDL_Init(SDL_INIT_AUDIO) != 0)
    {
        printf("SDL_Init audio error: %s\n", SDL_GetError());
        return 1;
    }

    // SDL_ShowCursor(SDL_DISABLE);

	// Setup window
	SDL_Window *win = SDL_CreateWindow("Hyphae",
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			1440, 980,
			SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

	// Setup renderer
	SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	// Setup font
	TTF_Init();
	TTF_Font *font = TTF_OpenFont("liberation.ttf", 12);
	if (!font)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error: Font", TTF_GetError(), win);
		return -666;
	}

    // Setup main loop
    srand(time(NULL));
    bool quit = false;
    bool do_iteration = true;

    // Main loop
    const float FPS_INTERVAL = 1.0f;
    Uint64 fps_start, fps_current, fps_frames = 0;

    fps_start = SDL_GetTicks();

    Graph graph;
#ifdef NODES_ON_HEAP
    graph.nodes = malloc(sizeof(Node) * MAX_NODES);
#endif
    graph.node_count = 0;
    graph.max_nodes = MAX_NODES;
    graph.initial_point_count = 3;

    UI ui;
    ui.font = font;
    ui.font_color = (SDL_Color){255, 255, 255, 255};
    ui.show = true;
    ui.quit = false;
    ui.do_iteration = true;
    ui.frames = 0;
    ui.render_intermediate = true;

    while (!ui.quit)
    {
        SDL_PumpEvents();
        get_input(&ui, &graph, ren);

        if (!ui.quit)
        {
            SDL_GetWindowSize(win, &graph.window.x, &graph.window.y);

            update(&ui, &graph);
            render(ren, graph, ui);

            fps_frames++;

            if (fps_start < SDL_GetTicks() - FPS_INTERVAL * 1000)
            {
                fps_start = SDL_GetTicks();
                fps_current = fps_frames;
                fps_frames = 0;

                ui.frames = fps_current;
                // printf("%I64d fps\n", fps_current);
            }
        }
    }

	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();
    return 0;
}
