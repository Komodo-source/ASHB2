
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Document</title>
</head>
<style>
  @import url("https://fonts.googleapis.com/css2?family=Roboto:ital,wght@0,100..900;1,100..900&display=swap");
        * {
            font-family: "Roboto", sans-serif;
        }
        main {
            margin-top: 2rem;
            margin-left: 2rem;
        }
        .top {
            text-align: center;
        }
        .reports > p {
            margin: 0rem 0 0rem 2rem;
        }
        .reports {
            transition: 0.7s;
            cursor: pointer;
            height: 2rem;
            padding-top: 1.1rem;
        }
        .reports:hover {
            border-left: 5px solid #111;
            background-color: #dadada;
        }
        .reports_list, .reports_list2{
            border: 1px solid #111;
            width: 20rem;
        }
        .global-container{
            display: flex;
        }
        .reports_list2{
            margin-left: 1rem;
        }
</style>
<body>
  <main>
    <div class="top">
      <h1>Recent report for A.S.H.B </h1>
      <p>/ last Update <?php $date ?> </p>
    </div>
    <div class="global-container">
    <div class="reports_list">
          <p style="text-align: center; font-size: 19px; font-weight: 500;" >Simulation Reports</p>
      <?php
        $path    = './';
        $files = scandir($path);
        $files = array_reverse($files);
		$date = "";
        $i = 0;
        //print_r($files);
        foreach($files as $reports){
          if(str_contains($reports, '.html')){
              if ($i == 0){
                  $date = date("F/d/Y H:i:s.", filectime($reports));
                  $i = 1;
              }
            ?>
            <div class="reports" onclick="window.location.href='./<?= htmlspecialchars($reports) ?>'">
              <p><?= htmlspecialchars($reports) ?></p>
            </div>
            <?php
          }
          }?>
	</div>
        <div class="reports_list2">
        <p style="text-align: center; font-size: 19px; font-weight: 500;" >Dev logs</p><?php
		foreach($files as $reports){
          if(str_contains($reports, '.txt')){
            ?>
            <div class="reports" onclick="window.location.href='./<?= htmlspecialchars($reports) ?>'">
              <p><?= htmlspecialchars($reports) ?></p>
            </div>
            <?php
          }
          }
          ?>
         </div>
  	</div>
    </div>
  </main>
</body>
</html>
