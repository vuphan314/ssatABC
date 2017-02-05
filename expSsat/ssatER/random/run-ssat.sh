COUNT=0
for k in $(seq 3 9)
do
  TESTCASES=$(ls expSsat/ssatER/random/${k}CNF/ssat)
  echo "name, time, value" >> randomER-ssat.csv
  for name in ${TESTCASES}
  do
    printf "%s, " ${name} >> randomER-ssat.csv
    timeout 1000 ./bin/ssat expSsat/ssatER/random/${k}CNF/ssat/${name} > tt.log && cat tt.log | grep "total time:\|Pr\[SAT\]" | awk 'NR==1{printf "%s, ", $4} NR==2{printf "%s\n", $3}' >> randomER-ssat.csv || echo "-, -" >> randomER-ssat.csv
  echo "case ${COUNT} done."
  COUNT=$((${COUNT} + 1))
  done
done
rm -f tt.log
