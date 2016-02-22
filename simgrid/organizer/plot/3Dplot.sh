#!/bin/bash


RESULT_PATH=../result/
FILE_NAME=lines.gp

if [ -f ./$FILE_NAME ]; then
    rm ./$FILE_NAME
fi

echo "# Gnuplot script file for plotting data
	  #set terminal png truecolor
	  #set output 'test.png'
      set   autoscale                        # scale axes automatically
      unset log                              # remove any log-scaling
      unset label                            # remove any previous labels
      #set xtic auto                          # set xtics automatically
      #set ytic auto                          # set ytics automatically
      #set ztic auto 
      set ticslevel 0
      #set format z '%.3f'  
      set mztics 0.01                 
      set xlabel 'computation\communicatoin'
      set ylabel 'alpha'
      set zlabel 'slow_down'
      set style data linespoints			 # for linespoint style
      # set style data histogram			 # for shitogram style
      # set style fill solid 1.00 border -1  # for shitogram style
      set isosample 70
      set dgrid3d 10 ,10 , 10 
      set hidden3d
      #set pm3d map
      set grid xtic ytics ztic" > $FILE_NAME

OFF=off.txt
ON=on.txt
printf "splot " >> $FILE_NAME
printf "'%s' using 1:2:3 title '%s', " $ON $ON >> $FILE_NAME                 # for linespoints style
printf "'%s' using 1:2:3 title '%s', " $OFF $OFF >> $FILE_NAME                 # for linespoints style

# counter=0
# size=$(ls $RESULT_PATH | wc -l )
# for file in $(ls $RESULT_PATH)
# do
# 	counter=$((counter+1))
# 	#echo $RESULT_PATH$file
# 	printf "'%s' using 2:3 title '%s'" $RESULT_PATH$file $file >> $FILE_NAME  			# for linespoints style
# 	#printf "'%s' using 3:xtic(2) title '%s'" $RESULT_PATH$file $file >> $FILE_NAME		# for histogram sytle
# 	if [ $counter != $size ] ;then
# 		printf ", " >> $FILE_NAME
# 	fi

# done
#cat plot.gp
gnuplot $FILE_NAME -p # -p force gnuplot to show the plot
#eog test.png &