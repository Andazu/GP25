#include <SDL3/SDL.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define ENABLE_DIAGNOSTICS
#define NUM_ASTEROIDS 10
#define NUM_BULLETS 64

#define VALIDATE(expression) if(!(expression)) { SDL_Log("%s\n", SDL_GetError()); }

#define NANOS(x)   (x)                // converts nanoseconds into nanoseconds
#define MICROS(x)  (NANOS(x) * 1000)  // converts microseconds into nanoseconds
#define MILLIS(x)  (MICROS(x) * 1000) // converts milliseconds into nanoseconds
#define SECONDS(x) (MILLIS(x) * 1000) // converts seconds into nanoseconds

#define NS_TO_MILLIS(x)  ((float)(x)/(float)1000000)    // converts nanoseconds to milliseconds (in floating point precision)
#define NS_TO_SECONDS(x) ((float)(x)/(float)1000000000) // converts nanoseconds to seconds (in floating point precision)

struct SDLContext
{
	SDL_Renderer* renderer;
	float window_w; // current window width after render zoom has been applied
	float window_h; // current window height after render zoom has been applied

	float delta;    // in seconds

	bool btn_pressed_up    = false;
	bool btn_pressed_down  = false;
	bool btn_pressed_left  = false;
	bool btn_pressed_right = false;
	bool btn_pressed_spacebar = false;
	bool btn_pressed_spacebar_prev = false; // we track the previous state of the spacebar and only fire a bullet on spawn on the transition
};

struct Entity
{
	SDL_FPoint   position;
	float        size;
	float        velocity;

	SDL_FRect    rect;
	SDL_Texture* texture_atlas;
	SDL_FRect    texture_rect;

	bool		 alive; // Added alive flag to check how many bullets are active
};

struct GameState
{
	Entity player;
	Entity asteroids[NUM_ASTEROIDS];
	Entity bullets[NUM_BULLETS]; // Added bullets with same Entity struct and new pool size

	SDL_Texture* texture_atlas;
};

static float distance_between(SDL_FPoint a, SDL_FPoint b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return SDL_sqrtf(dx*dx + dy*dy);
}

static float distance_between_sq(SDL_FPoint a, SDL_FPoint b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return dx*dx + dy*dy;
}

// hint: create a helper like this, then call it from collision & off-screen cases
static void respawn_asteroid(Entity* asteroid, SDLContext* ctx, float size, float speed_min, float speed_range) {
	asteroid->position.x = size + SDL_randf() * (ctx->window_w - size * 2);
	asteroid->position.y = -size;
	asteroid->velocity   = speed_min + SDL_randf() * speed_range;
}


static void init(SDLContext* context, GameState* game_state)
{
	// NOTE: these are a "design" parameter
	//       it is worth specifying a proper
	const float entity_size_world = 64;
	const float entity_size_texture = 128;
	const float player_speed = entity_size_world * 5;
	const int   player_sprite_coords_x = 4;
	const int   player_sprite_coords_y = 0;
	const float asteroid_speed_min = entity_size_world * 2;
	const float asteroid_speed_range = entity_size_world * 4;
	const int   asteroid_sprite_coords_x = 0;
	const int   asteroid_sprite_coords_y = 4;
	// Bullets
	const int bullet_speed = entity_size_world * 4;
	const int bullet_sprite_coords_x = 4;
	const int bullet_sprite_coords_y = 3;

	// load textures
	{
		int w = 0;
		int h = 0;
		int n = 0;
		unsigned char* pixels = stbi_load("data/kenney/simpleSpace_tilesheet_2.png", &w, &h, &n, 0);

		SDL_assert(pixels);

		// we don't really need this SDL_Surface, but it's the most conveninet way to create out texture
		// NOTE: out image has the color channels in RGBA order, but SDL_PIXELFORMAT
		//       behaves the opposite on little endina architectures (ie, most of them)
		//       we won't worry too much about that, just remember this if your textures looks wrong
		//       - check that the the color channels are actually what you expect (how many? how big? which order)
		//       - if everythig looks right, you might just need to flip the channel order, because of SDL
		SDL_Surface* surface = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, pixels, w * n);
		game_state->texture_atlas = SDL_CreateTextureFromSurface(context->renderer, surface);

		// NOTE: the texture will make a copy of the pixel data, so after creatio we can release both surface and pixel data
		SDL_DestroySurface(surface);
		stbi_image_free(pixels);
	}

	// character
	{
		game_state->player.position.x = context->window_w / 2 - entity_size_world / 2;
		game_state->player.position.y = context->window_h - entity_size_world * 2;
		game_state->player.size = entity_size_world;
		game_state->player.velocity = player_speed;
		game_state->player.texture_atlas = game_state->texture_atlas;

		// player size in the game world
		game_state->player.rect.w = game_state->player.size;
		game_state->player.rect.h = game_state->player.size;

		// sprite size (in the tilemap)
		game_state->player.texture_rect.w = entity_size_texture;
		game_state->player.texture_rect.h = entity_size_texture;
		// sprite position (in the tilemap)
		game_state->player.texture_rect.x = entity_size_texture * player_sprite_coords_x;
		game_state->player.texture_rect.y = entity_size_texture * player_sprite_coords_y;
	}

	// asteroids
	{
		for(int i = 0; i < NUM_ASTEROIDS; ++i)
		{
			Entity* asteroid_curr = &game_state->asteroids[i];

			asteroid_curr->position.x = entity_size_world + SDL_randf() * (context->window_w - entity_size_world * 2);
			asteroid_curr->position.y = -entity_size_world; // spawn asteroids off screen (almost)
			asteroid_curr->size       = entity_size_world;
			asteroid_curr->velocity   = asteroid_speed_min + SDL_randf() * asteroid_speed_range;
			asteroid_curr->texture_atlas = game_state->texture_atlas;

			asteroid_curr->rect.w = asteroid_curr->size;
			asteroid_curr->rect.h = asteroid_curr->size;

			asteroid_curr->texture_rect.w = entity_size_texture;
			asteroid_curr->texture_rect.h = entity_size_texture;

			asteroid_curr->texture_rect.x = entity_size_texture * asteroid_sprite_coords_x;
			asteroid_curr->texture_rect.y = entity_size_texture * asteroid_sprite_coords_y;
		}
	}

	// bullets
	{
		for (int i = 0; i < NUM_BULLETS; ++i)
		{
			Entity* bullet_curr = &game_state->bullets[i];

			// picking the atlas, aka the tilemap which we will select the sprite in later
			bullet_curr->texture_atlas = game_state->texture_atlas;

			// Active flag, false until we press spacebar ;)
			bullet_curr->alive = false;

			// picking the sprite in the tilemap
			bullet_curr->texture_rect.x = entity_size_texture * bullet_sprite_coords_x;
			bullet_curr->texture_rect.y = entity_size_texture * bullet_sprite_coords_y;

			// bullet size in the game world
			bullet_curr->size = entity_size_world * 0.33f; // bullet size, scaled it down
			bullet_curr->rect.w = bullet_curr->size;
			bullet_curr->rect.h = bullet_curr->size;

			// sprite size (in the tilemap)
			bullet_curr->texture_rect.w = entity_size_texture;
			bullet_curr->texture_rect.h = entity_size_texture;

			// velocity, speed of the bullet
			bullet_curr->velocity = bullet_speed;

		}
	}
}

static void update(SDLContext* context, GameState* game_state)
{
	bool restart_requested = false;

	// player
	{
		Entity* entity_player = &game_state->player;
		if(context->btn_pressed_up)
			entity_player->position.y -= context->delta * entity_player->velocity;
		if(context->btn_pressed_down)
			entity_player->position.y += context->delta * entity_player->velocity;
		if(context->btn_pressed_left)
			entity_player->position.x -= context->delta * entity_player->velocity;
		if(context->btn_pressed_right)
			entity_player->position.x += context->delta * entity_player->velocity;

		entity_player->rect.x = entity_player->position.x;
		entity_player->rect.y = entity_player->position.y;
		SDL_SetTextureColorMod(entity_player->texture_atlas, 0xFF, 0xFF, 0xFF);
		SDL_RenderTexture(
			context->renderer,
			entity_player->texture_atlas,
			&entity_player->texture_rect,
			&entity_player->rect
		);

		// if player position is outside window, stop them
		if (entity_player->position.x + entity_player->size >= context->window_w) {
			entity_player->position.x = context->window_w - entity_player->rect.w;
		}

		if (entity_player->position.x <= 0 ) {
			entity_player->position.x = 0;
		}
	}

	// asteroids
	{
		// how close an asteroid must be before categorizing it as "too close" (100 pixels. We square it because we can avoid doing the square root later)
		const float warning_distance_sq = 100*100;

		// how close an asteroid must be before triggering a collision (64 pixels. We square it because we can avoid doing the square root later)
		// the number 64 is obtained by summing togheter the "radii" of the sprites
		const float collision_distance_sq = 64*64;

		for(int i = 0; i < NUM_ASTEROIDS; ++i)
		{
			Entity* asteroid_curr = &game_state->asteroids[i];
			asteroid_curr->position.y += context->delta * asteroid_curr->velocity;

			asteroid_curr->rect.x = asteroid_curr->position.x;
			asteroid_curr->rect.y = asteroid_curr->position.y;

			float distance_sq = distance_between_sq(asteroid_curr->position, game_state->player.position);
			if(distance_sq < collision_distance_sq){
				SDL_SetTextureColorMod(asteroid_curr->texture_atlas, 0xFF, 0x00, 0x00);
				restart_requested = true;
			}
			else if(distance_sq < warning_distance_sq)
				SDL_SetTextureColorMod(asteroid_curr->texture_atlas, 0xCC, 0xCC, 0x00);
			else
				SDL_SetTextureColorMod(asteroid_curr->texture_atlas, 0xFF, 0xFF, 0xFF);

			SDL_RenderTexture(
				context->renderer,
				asteroid_curr->texture_atlas,
				&asteroid_curr->texture_rect,
				&asteroid_curr->rect
			);
		}
	}

	//bullets init
	{
		if (context->btn_pressed_spacebar && !context->btn_pressed_spacebar_prev) // we store the state of the spacebar from the last frame. If this condition is true and false (which would become true because its inverted) then it means "we are pressing spacebar, but it was not pressed last frame, meaning we just pressed it"
		{
			for (int i = 0; i < NUM_BULLETS; ++i)
			{
				Entity* bullet_curr = &game_state->bullets[i];

				// look for the first available bullet
				if (bullet_curr->alive == false)
				{
					// set to true, cuz were using it
					bullet_curr->alive = true;

					// set x position to middle of player
					bullet_curr->position.x = game_state->player.position.x + (game_state->player.size - bullet_curr->size) * 0.5;

					// set y position to just above the player
					bullet_curr->position.y = game_state->player.position.y - bullet_curr->size * 0.25f;

					bullet_curr->rect.x = bullet_curr->position.x;
					bullet_curr->rect.y = bullet_curr->position.y;

					break;
				}
			}
		}
	}

	// bullet pt 2: move, cull, draw
	{
		for (int i = 0; i < NUM_BULLETS; ++i)
		{
			Entity* bullet_curr = &game_state->bullets[i];
			SDL_FPoint bullet_center;

			if (bullet_curr->alive == false) continue;

			// move up
			bullet_curr->position.y -= context->delta * bullet_curr->velocity;

			bullet_curr->rect.x = bullet_curr->position.x;
			bullet_curr->rect.y = bullet_curr->position.y;

			// check collisions against every asteroid
			// get center positions of bullet instead of top left which is default
			bullet_center.x = bullet_curr->position.x + bullet_curr->size*0.5;
			bullet_center.y = bullet_curr->position.y + bullet_curr->size*0.5;

			// iterate over every asteroid and check for collision
			bool hit = false;
			for(int j = 0; j < NUM_ASTEROIDS; ++j) {
				Entity* asteroid_curr = &game_state->asteroids[j];
				SDL_FPoint asteroid_center;

				// get center positions of asteroid instead of top left which is default
				asteroid_center.x = asteroid_curr->position.x + asteroid_curr->size*0.5;
				asteroid_center.y = asteroid_curr->position.y + asteroid_curr->size*0.5;

				float distance = distance_between_sq(bullet_center, asteroid_center);
				float radius = (bullet_curr->size * 0.5f) + (asteroid_curr->size * 0.5f);

				//if the distance is less than the combined radius of the two targets, they are overlapping
				if (distance < radius * radius) {
					bullet_curr->alive = false;

					// respawn asteroid
					asteroid_curr->position.y = -asteroid_curr->size;
					asteroid_curr->position.x = asteroid_curr->size + SDL_randf() * (context->window_w - asteroid_curr->size * 2);

					hit = true;
					break;
				}
			}

			if (hit) continue;

			// if off-screen, set to false again
			if (bullet_curr->rect.y + bullet_curr->rect.h < 0) {
				bullet_curr->alive = false;
				continue;
			}

			// reset atlas tint
			SDL_SetTextureColorMod(bullet_curr->texture_atlas, 0xFF, 0xFF, 0xFF);

			// draw
			SDL_RenderTexture(
				context->renderer,
				bullet_curr->texture_atlas,
				&bullet_curr->texture_rect,
				&bullet_curr->rect
			);
		}
	}
	context->btn_pressed_spacebar_prev = context->btn_pressed_spacebar; // we store the previous state of the spacebar, so we cant hold it down if it was already held down last frame
	if (restart_requested) {
		context->btn_pressed_spacebar_prev = false;
	}
}

int main(void)
{
	SDLContext context = { 0 };
	GameState game_state = { 0 };

	float window_w = 600;
	float window_h = 800;
	int target_framerate = SECONDS(1) / 60;

	SDL_Window* window = SDL_CreateWindow("E01 - Rendering", window_w, window_h, 0);
	context.renderer = SDL_CreateRenderer(window, NULL);
	context.window_w = window_w;
	context.window_h = window_h;

	// increase the zoom to make debug text more legible
	// (ie, on the class projector, we will usually use 2)
	{
		float zoom = 1;
		context.window_w /= zoom;
		context.window_h /= zoom;
		SDL_SetRenderScale(context.renderer, zoom, zoom);
	}

	bool quit = false;

	SDL_Time walltime_frame_beg;
	SDL_Time walltime_work_end;
	SDL_Time walltime_frame_end;
	SDL_Time time_elapsed_frame;
	SDL_Time time_elapsed_work;

	init(&context, &game_state);

	SDL_GetCurrentTime(&walltime_frame_beg);
	while(!quit)
	{
		// input
		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
				case SDL_EVENT_QUIT:
					quit = true;
					break;

				case SDL_EVENT_KEY_UP:
				case SDL_EVENT_KEY_DOWN:
					if(event.key.key == SDLK_W)
						context.btn_pressed_up = event.key.down;
					if(event.key.key == SDLK_A)
						context.btn_pressed_left = event.key.down;
					if(event.key.key == SDLK_S)
						context.btn_pressed_down = event.key.down;
					if(event.key.key == SDLK_D)
						context.btn_pressed_right = event.key.down;
					if(event.key.key == SDLK_SPACE)
						context.btn_pressed_spacebar = event.key.down;
			}
		}

		// clear screen
		SDL_SetRenderDrawColor(context.renderer, 0x00, 0x00, 0x00, 0x00);
		SDL_RenderClear(context.renderer);

		update(&context, &game_state);

		SDL_GetCurrentTime(&walltime_work_end);
		time_elapsed_work = walltime_work_end - walltime_frame_beg;

		if(target_framerate > time_elapsed_work)
		{
			SDL_DelayPrecise(target_framerate - time_elapsed_work);
		}

		SDL_GetCurrentTime(&walltime_frame_end);
		time_elapsed_frame = walltime_frame_end - walltime_frame_beg;

		context.delta = NS_TO_SECONDS(time_elapsed_frame);

#ifdef ENABLE_DIAGNOSTICS
		SDL_SetRenderDrawColor(context.renderer, 0xFF, 0xFF, 0xFF, 0xFF);
		SDL_RenderDebugTextFormat(context.renderer, 10.0f, 10.0f, "elapsed (frame): %9.6f ms", NS_TO_MILLIS(time_elapsed_frame));
		SDL_RenderDebugTextFormat(context.renderer, 10.0f, 20.0f, "elapsed(work)  : %9.6f ms", NS_TO_MILLIS(time_elapsed_work));
#endif

		// render
		SDL_RenderPresent(context.renderer);

		walltime_frame_beg = walltime_frame_end;
	}

	return 0;
};