# color_reducer
A small utility to reduce an image to a certain number of colors using a user provided palette

Usage: color_reducer input_file_name palette_image_file maximum_color_number output_file_name

For example: color_reducer main_menu.png sms_palette.png 16 main_menu_16.png

This call takes main_menu.png file and creates a new file (main_menu_16.png) where each pixel color is replaced by the closest color from the set extracted from sms_palette.png file and using at most 16 colors of this set.
