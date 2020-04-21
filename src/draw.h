
typedef struct {
    vec2 center;
    int radius;
} Circle;

bool do_circles_collide(Circle a, Circle b)
{
    float dx = (a.center.x - b.center.x); 
    float dy = (a.center.y - b.center.y);
    float distance_between_a_and_b = sqrt((dx*dx) + (dy*dy));
    return (distance_between_a_and_b < a.radius + b.radius);
}

void draw_circle(SDL_Renderer *renderer, Circle circle, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int w = 0; w < circle.radius * 2; w += 1)
    {
        for (int h = 0; h < circle.radius * 2; h += 1)
        {
            int dx = circle.radius - w;
            int dy = circle.radius - h;
            if ((dx*dx + dy*dy) <= (circle.radius * circle.radius))
            {
                SDL_RenderDrawPoint(renderer, circle.center.x + dx, circle.center.y + dy);
            }
        }
    }
}

void draw_text(SDL_Renderer *renderer, int x, int y, char *string, TTF_Font *font, SDL_Color font_color) {
    SDL_Surface *surface = TTF_RenderText_Blended(font, string, font_color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    int x_from_texture, y_from_texture;
    SDL_QueryTexture(texture, NULL, NULL, &x_from_texture, &y_from_texture);
    SDL_Rect rect = {x, y, x_from_texture, y_from_texture};

    SDL_RenderCopy(renderer, texture, NULL, &rect);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}
