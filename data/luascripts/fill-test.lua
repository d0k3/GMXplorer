print("Doing fill test...")
fs.fill_file("9:/test.txt", 0, 16, string.byte("A"))
print("Doing second fill test...")
fs.fill_file("9:/test.txt", 4, 8, string.byte("B"))
ui.echo("Done")