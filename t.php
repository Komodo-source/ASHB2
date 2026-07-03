<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Document</title>
</head>
  <script src="https://cdn.jsdelivr.net/npm/marked/lib/marked.umd.js"></script>
<body>
  <div class="main" id="main">
    <p>sdq</p>
  <?php
    $file_name = $_GET["file_name"];
    echo "fd";
    $file = file_get_contents($file_name, true);
    echo $file_name;
    echo $file ;
    echo("hello");
  ?><script>
    document.getElementById('main').innerHTML =
    marked.parse('<?php htmlspecialchars($file) ?>');
    </script>

  </div>

</body>
</html>
