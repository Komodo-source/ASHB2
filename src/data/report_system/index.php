
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Document</title>
</head>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Roboto:ital,wght@0,100..900;1,100..900&display=swap');
  * {
    font-family: "Roboto", sans-serif;
  }
  main{
    margin-top: 2rem;
    margin-left: 2rem;
  }
  .top{
    text-align: center;
  }
  .reports > p {
    margin: 0rem 0 0rem 2rem;
  }
  .reports{
  transition: 0.7s;
    cursor: pointer;
    height: 2rem;
    padding-top: 1.1rem;
  }
  .reports:hover{
    border-left: 5px solid #00bbff;
    background-color: #dadada;
  }
  .reports_list{
    border: 1px solid #111;
    width: 20rem;

  }
</style>
<body>
  <main>
    <div class="top">
      <h1>Recent report for ASHB</h1>
      <p> /// last Update 21:50 // 21/03/26</p>
    </div>
    <div class="reports_list">
      <?php
        $path    = './';
        $files = scandir($path);
        $files = array_reverse($files);
        //print_r($files);
        foreach($files as $reports){
          if(str_contains($reports, '.html')){
            ?>
            <div class="reports" onclick="window.location.href='./<?= htmlspecialchars($reports) ?>'">
              <p><?= htmlspecialchars($reports) ?></p>
            </div>
            <?php
          }
          }
          ?>
    </div>
  </main>
</body>
</html>
