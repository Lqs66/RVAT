# Testing Results

✓: no violation found; ✗: violation found; Timeout: testing time limit reached (shown in the Testing time column); —: inconclusive due to timeout (in Result) or no violation detected across tested depths 3, 4, and 5 (in First violation depth). Modeling time and testing time are reported in seconds to two decimal places.

<table>
<thead>
<tr>
<th rowspan="2">Policy ID</th>
<th colspan="5">Depth 3</th>
<th colspan="5">Depth 4</th>
<th colspan="5">Depth 5</th>
<th rowspan="2">First violation depth<br>(tested: 3, 4, 5)</th>
</tr>
<tr>
<th>Result</th><th>#Locations</th><th>#Transitions</th><th>Modeling time (s)</th><th>Testing time (s)</th>
<th>Result</th><th>#Locations</th><th>#Transitions</th><th>Modeling time (s)</th><th>Testing time (s)</th>
<th>Result</th><th>#Locations</th><th>#Transitions</th><th>Modeling time (s)</th><th>Testing time (s)</th>
</tr>
</thead>
<tbody>
<tr><td><code>PX.RTL2</code></td><td>✓</td><td>1565</td><td>1748</td><td>9.41</td><td>77.39</td><td>✓</td><td>2489</td><td>2805</td><td>9.24</td><td>106.63</td><td>✓</td><td>4205</td><td>4762</td><td>9.32</td><td>196.98</td><td>—</td></tr>
<tr><td><code>PX.RTL3</code></td><td>✓</td><td>1307</td><td>1460</td><td>9.07</td><td>94.83</td><td>✓</td><td>1749</td><td>1964</td><td>9.17</td><td>106.84</td><td>✓</td><td>2410</td><td>2743</td><td>9.52</td><td>199.66</td><td>—</td></tr>
<tr><td><code>PX.RTL4</code></td><td>✓</td><td>1307</td><td>1460</td><td>9.12</td><td>92.46</td><td>✓</td><td>1749</td><td>1964</td><td>9.21</td><td>100.12</td><td>✓</td><td>2410</td><td>2743</td><td>9.17</td><td>126.82</td><td>—</td></tr>
<tr><td><code>PX_RTL_P1</code></td><td>✓</td><td>1565</td><td>1748</td><td>9.82</td><td>78.23</td><td>✓</td><td>2489</td><td>2805</td><td>9.28</td><td>95.06</td><td>✓</td><td>4205</td><td>4762</td><td>9.32</td><td>149.28</td><td>—</td></tr>
<tr><td><code>PX_RTL_P2</code></td><td>✓</td><td>1565</td><td>1748</td><td>9.32</td><td>82.88</td><td>✓</td><td>2489</td><td>2805</td><td>9.32</td><td>105.56</td><td>✓</td><td>4205</td><td>4762</td><td>9.47</td><td>165.67</td><td>—</td></tr>
<tr><td><code>PX_RTL_P3</code></td><td>✓</td><td>1565</td><td>1748</td><td>9.24</td><td>83.93</td><td>✓</td><td>2489</td><td>2805</td><td>9.27</td><td>109.34</td><td>✓</td><td>4205</td><td>4762</td><td>9.61</td><td>185.36</td><td>—</td></tr>
<tr><td><code>PX.RTL5</code></td><td>✓</td><td>1444</td><td>1716</td><td>9.07</td><td>85.25</td><td>✓</td><td>1868</td><td>2196</td><td>9.52</td><td>94.60</td><td>✓</td><td>2564</td><td>2960</td><td>9.21</td><td>122.37</td><td>—</td></tr>
<tr><td><code>PX.LAND1</code></td><td>✓</td><td>1473</td><td>1635</td><td>9.58</td><td>59.05</td><td>✓</td><td>2033</td><td>2283</td><td>9.21</td><td>75.05</td><td>✓</td><td>2518</td><td>2843</td><td>9.38</td><td>85.60</td><td>—</td></tr>
<tr><td><code>PX.ORBIT1</code></td><td>✓</td><td>2492</td><td>2741</td><td>9.27</td><td>117.89</td><td>✓</td><td>3917</td><td>4378</td><td>9.52</td><td>160.58</td><td>✓</td><td>5340</td><td>5982</td><td>9.84</td><td>189.68</td><td>—</td></tr>
<tr><td><code>PX.ORBIT2</code></td><td>✓</td><td>2492</td><td>2741</td><td>9.17</td><td>117.23</td><td>✓</td><td>3917</td><td>4378</td><td>9.27</td><td>160.76</td><td>✓</td><td>5340</td><td>5982</td><td>9.32</td><td>191.22</td><td>—</td></tr>
<tr><td><code>PX.ORBIT3</code></td><td>✓</td><td>2492</td><td>2741</td><td>9.17</td><td>122.88</td><td>✓</td><td>3917</td><td>4378</td><td>9.37</td><td>196.73</td><td>✗</td><td>5340</td><td>5982</td><td>9.44</td><td>229.64</td><td>5</td></tr>
<tr><td><code>PX.ORBIT4</code></td><td>✓</td><td>2492</td><td>2741</td><td>9.21</td><td>120.15</td><td>✓</td><td>3917</td><td>4378</td><td>9.58</td><td>195.57</td><td>✗</td><td>5340</td><td>5982</td><td>9.38</td><td>232.12</td><td>5</td></tr>
<tr><td><code>PX.ORBIT6</code></td><td>✓</td><td>2492</td><td>2741</td><td>9.24</td><td>101.37</td><td>✓</td><td>3917</td><td>4378</td><td>9.32</td><td>135.55</td><td>✓</td><td>5340</td><td>5982</td><td>9.32</td><td>180.92</td><td>—</td></tr>
<tr><td><code>PX_ORBIT_P1</code></td><td>✗</td><td>2492</td><td>2741</td><td>9.81</td><td>99.61</td><td>✗</td><td>3917</td><td>4378</td><td>9.37</td><td>134.55</td><td>✗</td><td>5340</td><td>5982</td><td>9.38</td><td>180.95</td><td>3</td></tr>
<tr><td><code>PX_ORBIT_P2</code></td><td>✗</td><td>2492</td><td>2741</td><td>9.17</td><td>101.46</td><td>✗</td><td>3917</td><td>4378</td><td>9.61</td><td>137.17</td><td>✗</td><td>5340</td><td>5982</td><td>9.61</td><td>185.23</td><td>3</td></tr>
<tr><td><code>PX.ALTITUDE1</code></td><td>✓</td><td>1238</td><td>1409</td><td>9.17</td><td>76.12</td><td>✓</td><td>1338</td><td>1526</td><td>9.21</td><td>80.60</td><td>✓</td><td>1383</td><td>1579</td><td>9.32</td><td>76.25</td><td>—</td></tr>
<tr><td><code>PX_ALTITUDE_P1</code></td><td>✓</td><td>87</td><td>96</td><td>9.12</td><td>9.79</td><td>✓</td><td>87</td><td>96</td><td>9.12</td><td>9.73</td><td>✓</td><td>87</td><td>96</td><td>9.12</td><td>10.03</td><td>—</td></tr>
<tr><td><code>PX_ALTITUDE_P2</code></td><td>✓</td><td>87</td><td>96</td><td>9.07</td><td>9.69</td><td>✓</td><td>87</td><td>96</td><td>9.37</td><td>9.88</td><td>✓</td><td>87</td><td>96</td><td>9.12</td><td>9.83</td><td>—</td></tr>
<tr><td><code>PX.POSITION1</code></td><td>✓</td><td>2036</td><td>2269</td><td>9.12</td><td>127.39</td><td>✓</td><td>3333</td><td>3746</td><td>9.52</td><td>182.69</td><td>✓</td><td>4446</td><td>5027</td><td>9.72</td><td>213.41</td><td>—</td></tr>
<tr><td><code>PX_POSITION_P1</code></td><td>✓</td><td>1405</td><td>1571</td><td>9.27</td><td>50.96</td><td>✗</td><td>2188</td><td>2444</td><td>9.32</td><td>106.47</td><td>✗</td><td>3328</td><td>3766</td><td>9.27</td><td>149.40</td><td>4</td></tr>
<tr><td><code>PX_POSITION_P2</code></td><td>✓</td><td>1405</td><td>1571</td><td>9.21</td><td>74.40</td><td>✗</td><td>2188</td><td>2444</td><td>9.32</td><td>103.02</td><td>✗</td><td>3328</td><td>3766</td><td>9.22</td><td>143.55</td><td>4</td></tr>
<tr><td><code>PX.HOLD1</code></td><td>✗</td><td>5624</td><td>6198</td><td>9.32</td><td>341.52</td><td>✗</td><td>9884</td><td>11048</td><td>9.92</td><td>1699.20</td><td>✗</td><td>14058</td><td>15768</td><td>9.72</td><td>5785.28</td><td>3</td></tr>
<tr><td><code>PX.HOLD2</code></td><td>✓</td><td>444</td><td>497</td><td>9.12</td><td>64.07</td><td>✓</td><td>491</td><td>547</td><td>9.21</td><td>67.20</td><td>✓</td><td>500</td><td>556</td><td>9.07</td><td>66.89</td><td>—</td></tr>
<tr><td><code>PX.TAKEOFF1</code></td><td>✗</td><td>418</td><td>462</td><td>9.07</td><td>64.64</td><td>✗</td><td>464</td><td>510</td><td>9.37</td><td>64.89</td><td>✗</td><td>584</td><td>652</td><td>9.41</td><td>61.75</td><td>3</td></tr>
<tr><td><code>PX.TAKEOFF2</code></td><td>✓</td><td>17</td><td>20</td><td>9.12</td><td>10.34</td><td>✓</td><td>17</td><td>20</td><td>9.17</td><td>9.59</td><td>✓</td><td>17</td><td>20</td><td>9.04</td><td>9.54</td><td>—</td></tr>
<tr><td><code>PX_RCFS_P1</code></td><td>✗</td><td>437</td><td>477</td><td>25.64</td><td>34.91</td><td>✗</td><td>584</td><td>637</td><td>24.63</td><td>34.66</td><td>✗</td><td>866</td><td>967</td><td>25.74</td><td>37.04</td><td>3</td></tr>
<tr><td><code>PX.GPS.FS1</code></td><td>✗</td><td>41</td><td>48</td><td>12.73</td><td>9.16</td><td>✗</td><td>41</td><td>48</td><td>12.61</td><td>8.88</td><td>✗</td><td>41</td><td>48</td><td>12.68</td><td>8.63</td><td>3</td></tr>
<tr><td><code>PX.GPS.FS2</code></td><td>✗</td><td>5624</td><td>6198</td><td>9.41</td><td>330.14</td><td>✗</td><td>9884</td><td>11048</td><td>9.67</td><td>1731.44</td><td>✗</td><td>14058</td><td>15768</td><td>9.72</td><td>5918.21</td><td>3</td></tr>
<tr><td><code>PX.GPS.FS3</code></td><td>✗</td><td>5624</td><td>6198</td><td>9.41</td><td>339.72</td><td>✗</td><td>9884</td><td>11048</td><td>9.72</td><td>1744.03</td><td>✗</td><td>14058</td><td>15768</td><td>9.55</td><td>6003.17</td><td>3</td></tr>
<tr><td><code>PX_BRAKE_P1</code></td><td>✗</td><td>5624</td><td>6198</td><td>9.52</td><td>365.41</td><td>✗</td><td>9884</td><td>11048</td><td>9.57</td><td>688.90</td><td>✗</td><td>14058</td><td>15768</td><td>10.12</td><td>3309.35</td><td>3</td></tr>
<tr><td><code>AP_RTL_P1</code></td><td>✓</td><td>10010</td><td>11256</td><td>8.27</td><td>626.97</td><td>—</td><td>29619</td><td>33114</td><td>9.32</td><td>Timeout</td><td>—</td><td>65839</td><td>73971</td><td>12.01</td><td>Timeout</td><td>—</td></tr>
<tr><td><code>AP_RTL_P2</code></td><td>✓</td><td>10010</td><td>11256</td><td>8.38</td><td>628.29</td><td>—</td><td>29619</td><td>33114</td><td>9.37</td><td>Timeout</td><td>—</td><td>65839</td><td>73971</td><td>11.78</td><td>Timeout</td><td>—</td></tr>
<tr><td><code>A.RTL2</code></td><td>✓</td><td>10010</td><td>11256</td><td>8.12</td><td>1102.39</td><td>—</td><td>29619</td><td>33114</td><td>9.21</td><td>Timeout</td><td>—</td><td>65839</td><td>73971</td><td>11.81</td><td>Timeout</td><td>—</td></tr>
<tr><td><code>AP_RTL_P3</code></td><td>✓</td><td>10010</td><td>11256</td><td>8.37</td><td>1303.09</td><td>—</td><td>29619</td><td>33114</td><td>9.21</td><td>Timeout</td><td>—</td><td>65839</td><td>73971</td><td>11.68</td><td>Timeout</td><td>—</td></tr>
<tr><td><code>A.RTL3</code></td><td>✓</td><td>10010</td><td>11256</td><td>8.12</td><td>1077.11</td><td>—</td><td>29619</td><td>33114</td><td>9.27</td><td>Timeout</td><td>—</td><td>65839</td><td>73971</td><td>11.68</td><td>Timeout</td><td>—</td></tr>
<tr><td><code>A.RTL4</code></td><td>✓</td><td>2863</td><td>3242</td><td>7.92</td><td>120.80</td><td>✓</td><td>9045</td><td>10129</td><td>8.32</td><td>842.01</td><td>✓</td><td>21790</td><td>24512</td><td>8.92</td><td>8697.94</td><td>—</td></tr>
<tr><td><code>A.LAND1</code></td><td>✗</td><td>2544</td><td>2940</td><td>7.84</td><td>104.42</td><td>✗</td><td>5688</td><td>6675</td><td>8.07</td><td>199.58</td><td>✗</td><td>9187</td><td>10693</td><td>8.07</td><td>732.12</td><td>3</td></tr>
<tr><td><code>A.LAND2</code></td><td>✗</td><td>2544</td><td>2940</td><td>8.12</td><td>107.45</td><td>✗</td><td>5688</td><td>6675</td><td>7.97</td><td>203.16</td><td>✗</td><td>9187</td><td>10693</td><td>8.17</td><td>720.24</td><td>3</td></tr>
<tr><td><code>AP_LAND_P1</code></td><td>✓</td><td>2990</td><td>3331</td><td>7.92</td><td>103.03</td><td>✓</td><td>8536</td><td>9517</td><td>8.47</td><td>484.56</td><td>✓</td><td>20725</td><td>23212</td><td>9.21</td><td>7905.33</td><td>—</td></tr>
<tr><td><code>A.FLIP1</code></td><td>✗</td><td>168</td><td>186</td><td>7.92</td><td>57.90</td><td>✗</td><td>197</td><td>221</td><td>7.77</td><td>59.04</td><td>✗</td><td>340</td><td>398</td><td>8.01</td><td>74.95</td><td>3</td></tr>
<tr><td><code>A.FLIP2</code></td><td>✓</td><td>7180</td><td>7985</td><td>8.07</td><td>902.03</td><td>✓</td><td>14493</td><td>16337</td><td>8.72</td><td>6851.43</td><td>✓</td><td>25286</td><td>28867</td><td>9.27</td><td>29881.10</td><td>—</td></tr>
<tr><td><code>A.FLIPGeneral</code></td><td>✓</td><td>7180</td><td>7985</td><td>12.23</td><td>408.26</td><td>✓</td><td>14493</td><td>16337</td><td>12.18</td><td>4174.44</td><td>✓</td><td>25286</td><td>28867</td><td>12.78</td><td>31468.74</td><td>—</td></tr>
<tr><td><code>A.FLIP3</code></td><td>✓</td><td>7180</td><td>7985</td><td>12.18</td><td>275.01</td><td>✓</td><td>14493</td><td>16337</td><td>12.48</td><td>3160.47</td><td>✓</td><td>25286</td><td>28867</td><td>12.88</td><td>22039.66</td><td>—</td></tr>
<tr><td><code>A.ALT_HOLD1</code></td><td>✓</td><td>1099</td><td>1260</td><td>7.61</td><td>75.45</td><td>✓</td><td>1206</td><td>1381</td><td>8.07</td><td>77.12</td><td>✓</td><td>1272</td><td>1454</td><td>7.61</td><td>78.78</td><td>—</td></tr>
<tr><td><code>AP_ALTHOLD_P1</code></td><td>✗</td><td>4599</td><td>5137</td><td>8.01</td><td>199.33</td><td>✗</td><td>8061</td><td>9065</td><td>8.57</td><td>874.76</td><td>✗</td><td>12887</td><td>14652</td><td>8.38</td><td>3085.09</td><td>3</td></tr>
<tr><td><code>A.ALT_HOLD2</code></td><td>✗</td><td>6259</td><td>7131</td><td>8.17</td><td>254.05</td><td>✗</td><td>9546</td><td>10851</td><td>8.37</td><td>943.29</td><td>✗</td><td>17822</td><td>20971</td><td>8.64</td><td>6443.53</td><td>3</td></tr>
<tr><td><code>AP_POSHOLD_P1</code></td><td>✓</td><td>8089</td><td>8923</td><td>8.32</td><td>334.92</td><td>✓</td><td>14605</td><td>16255</td><td>8.87</td><td>2849.50</td><td>✓</td><td>22736</td><td>25505</td><td>9.12</td><td>7181.85</td><td>—</td></tr>
<tr><td><code>AP_POSHOLD_P2</code></td><td>✓</td><td>8089</td><td>8923</td><td>8.07</td><td>338.57</td><td>✓</td><td>14605</td><td>16255</td><td>8.97</td><td>2737.91</td><td>✓</td><td>22736</td><td>25505</td><td>9.01</td><td>7815.07</td><td>—</td></tr>
<tr><td><code>AP_POSHOLD_P3</code></td><td>✗</td><td>8089</td><td>8923</td><td>8.17</td><td>819.55</td><td>✗</td><td>14605</td><td>16255</td><td>8.52</td><td>4040.39</td><td>✗</td><td>22736</td><td>25505</td><td>9.04</td><td>14171.70</td><td>3</td></tr>
<tr><td><code>A.CIRCLE1</code></td><td>✓</td><td>6354</td><td>7023</td><td>8.17</td><td>287.87</td><td>✓</td><td>13421</td><td>14910</td><td>8.61</td><td>3332.46</td><td>✓</td><td>22751</td><td>25495</td><td>9.21</td><td>14939.22</td><td>—</td></tr>
<tr><td><code>A.CIRCLE2</code></td><td>✓</td><td>6354</td><td>7023</td><td>8.01</td><td>239.79</td><td>✓</td><td>13421</td><td>14910</td><td>8.06</td><td>3201.01</td><td>✓</td><td>22751</td><td>25495</td><td>9.33</td><td>15021.87</td><td>—</td></tr>
<tr><td><code>A.CIRCLE3</code></td><td>✓</td><td>6354</td><td>7023</td><td>8.24</td><td>283.82</td><td>✓</td><td>13421</td><td>14910</td><td>8.34</td><td>3413.19</td><td>✓</td><td>22751</td><td>25495</td><td>9.72</td><td>14197.09</td><td>—</td></tr>
<tr><td><code>A.CIRCLE4</code></td><td>✓</td><td>6354</td><td>7023</td><td>8.02</td><td>285.92</td><td>✓</td><td>13421</td><td>14910</td><td>9.00</td><td>3193.96</td><td>✓</td><td>22751</td><td>25495</td><td>9.05</td><td>15443.13</td><td>—</td></tr>
<tr><td><code>A.CIRCLE5</code></td><td>✓</td><td>6354</td><td>7023</td><td>8.07</td><td>285.15</td><td>✓</td><td>13421</td><td>14910</td><td>8.71</td><td>3277.43</td><td>✓</td><td>22751</td><td>25495</td><td>9.13</td><td>14992.37</td><td>—</td></tr>
<tr><td><code>A.CIRCLE6</code></td><td>✓</td><td>6354</td><td>7023</td><td>8.02</td><td>284.35</td><td>✓</td><td>13421</td><td>14910</td><td>8.12</td><td>3324.21</td><td>✓</td><td>22751</td><td>25495</td><td>8.89</td><td>15290.64</td><td>—</td></tr>
<tr><td><code>A.CIRCLE7</code></td><td>✓</td><td>6354</td><td>7023</td><td>8.01</td><td>446.71</td><td>✓</td><td>13421</td><td>14910</td><td>8.02</td><td>3585.74</td><td>✓</td><td>22751</td><td>25495</td><td>9.18</td><td>14470.62</td><td>—</td></tr>
<tr><td><code>A.AUTO1</code></td><td>✓</td><td>4397</td><td>4875</td><td>7.97</td><td>92.14</td><td>✓</td><td>9907</td><td>11020</td><td>8.37</td><td>247.57</td><td>✓</td><td>16791</td><td>18787</td><td>8.67</td><td>1596.64</td><td>—</td></tr>
<tr><td><code>AP_DRIFT_P1</code></td><td>✓</td><td>1822</td><td>2012</td><td>7.81</td><td>75.84</td><td>✓</td><td>2993</td><td>3318</td><td>8.37</td><td>110.28</td><td>✓</td><td>4616</td><td>5140</td><td>8.17</td><td>142.22</td><td>—</td></tr>
<tr><td><code>A.DRIFT1</code></td><td>✓</td><td>1739</td><td>2005</td><td>7.81</td><td>97.42</td><td>✓</td><td>3163</td><td>3636</td><td>7.97</td><td>138.18</td><td>✓</td><td>6953</td><td>8129</td><td>8.12</td><td>753.59</td><td>—</td></tr>
<tr><td><code>AP_DRIFT_P2</code></td><td>✓</td><td>1822</td><td>2012</td><td>7.84</td><td>51.22</td><td>✓</td><td>2993</td><td>3318</td><td>7.92</td><td>57.31</td><td>✓</td><td>4616</td><td>5140</td><td>8.01</td><td>56.13</td><td>—</td></tr>
<tr><td><code>A.LOITER1</code></td><td>✗</td><td>11369</td><td>12371</td><td>8.57</td><td>3109.43</td><td>✗</td><td>21839</td><td>24090</td><td>8.97</td><td>14380.69</td><td>—</td><td>34201</td><td>38082</td><td>9.77</td><td>Timeout</td><td>3</td></tr>
<tr><td><code>AP_LOITER_P1</code></td><td>✗</td><td>11369</td><td>12371</td><td>8.24</td><td>1875.62</td><td>✗</td><td>21839</td><td>24090</td><td>8.87</td><td>13906.04</td><td>—</td><td>34201</td><td>38082</td><td>9.64</td><td>Timeout</td><td>3</td></tr>
<tr><td><code>AP_LOITER_P2</code></td><td>✓</td><td>1543</td><td>1729</td><td>8.01</td><td>16.20</td><td>✓</td><td>3437</td><td>3868</td><td>8.07</td><td>35.27</td><td>✓</td><td>6238</td><td>6968</td><td>8.01</td><td>49.65</td><td>—</td></tr>
<tr><td><code>AP_LOITER_P3</code></td><td>✓</td><td>1849</td><td>2033</td><td>7.92</td><td>22.13</td><td>✓</td><td>2567</td><td>2874</td><td>7.81</td><td>31.36</td><td>✓</td><td>3288</td><td>3705</td><td>7.87</td><td>31.98</td><td>—</td></tr>
<tr><td><code>A.GUIDED1</code></td><td>✓</td><td>4407</td><td>4886</td><td>8.02</td><td>99.64</td><td>✓</td><td>9917</td><td>11031</td><td>8.21</td><td>263.42</td><td>✓</td><td>16801</td><td>18798</td><td>8.81</td><td>1635.41</td><td>—</td></tr>
<tr><td><code>A.SPORT1</code></td><td>✗</td><td>4599</td><td>5137</td><td>8.32</td><td>241.78</td><td>✗</td><td>8061</td><td>9065</td><td>8.27</td><td>938.67</td><td>✗</td><td>12887</td><td>14652</td><td>8.52</td><td>3148.10</td><td>3</td></tr>
<tr><td><code>AP_ACRO_P1</code></td><td>✓</td><td>3014</td><td>3342</td><td>8.47</td><td>172.10</td><td>✗</td><td>4395</td><td>4868</td><td>8.07</td><td>290.46</td><td>✗</td><td>5793</td><td>6427</td><td>8.07</td><td>782.17</td><td>4</td></tr>
<tr><td><code>AP_ACRO_P2</code></td><td>✓</td><td>3014</td><td>3342</td><td>7.84</td><td>178.07</td><td>✗</td><td>4395</td><td>4868</td><td>8.04</td><td>333.70</td><td>✗</td><td>5793</td><td>6427</td><td>8.01</td><td>598.77</td><td>4</td></tr>
<tr><td><code>AP_STABILIZE_P1</code></td><td>✓</td><td>1830</td><td>2015</td><td>7.77</td><td>85.57</td><td>✓</td><td>3006</td><td>3326</td><td>7.97</td><td>119.78</td><td>✓</td><td>4629</td><td>5148</td><td>7.97</td><td>146.07</td><td>—</td></tr>
<tr><td><code>AP_STABILIZE_P2</code></td><td>✓</td><td>1830</td><td>2015</td><td>7.97</td><td>86.95</td><td>✓</td><td>3006</td><td>3326</td><td>7.92</td><td>120.75</td><td>✓</td><td>4629</td><td>5148</td><td>8.01</td><td>147.99</td><td>—</td></tr>
<tr><td><code>AP_ZIGZAG_P1</code></td><td>✓</td><td>7039</td><td>7571</td><td>8.04</td><td>441.01</td><td>✗</td><td>16712</td><td>18251</td><td>8.57</td><td>6145.83</td><td>—</td><td>30885</td><td>34078</td><td>9.41</td><td>Timeout</td><td>4</td></tr>
<tr><td><code>A.RC.FS1</code></td><td>✗</td><td>2695</td><td>3130</td><td>8.12</td><td>96.23</td><td>✗</td><td>7608</td><td>8760</td><td>8.07</td><td>442.02</td><td>✗</td><td>15255</td><td>17422</td><td>8.52</td><td>4859.32</td><td>3</td></tr>
<tr><td><code>A.RC.FS2</code></td><td>✗</td><td>1239</td><td>1415</td><td>7.82</td><td>79.29</td><td>✗</td><td>2466</td><td>2810</td><td>7.87</td><td>112.25</td><td>✗</td><td>5925</td><td>6808</td><td>8.04</td><td>252.54</td><td>3</td></tr>
<tr><td><code>AP_RCFS_P1</code></td><td>✓</td><td>2695</td><td>3130</td><td>8.01</td><td>102.33</td><td>✓</td><td>7608</td><td>8760</td><td>8.27</td><td>531.58</td><td>✓</td><td>15255</td><td>17422</td><td>9.92</td><td>5377.52</td><td>—</td></tr>
<tr><td><code>AP_GCSFS_P1</code></td><td>✗</td><td>2761</td><td>3201</td><td>8.01</td><td>103.65</td><td>✗</td><td>7692</td><td>8849</td><td>8.17</td><td>678.80</td><td>✗</td><td>15351</td><td>17523</td><td>8.67</td><td>5725.71</td><td>3</td></tr>
<tr><td><code>AP_GCSFS_P2</code></td><td>✓</td><td>2761</td><td>3201</td><td>7.92</td><td>98.34</td><td>✓</td><td>7692</td><td>8849</td><td>8.21</td><td>494.03</td><td>✓</td><td>15351</td><td>17523</td><td>8.57</td><td>5021.00</td><td>—</td></tr>
<tr><td><code>AP_BATTFS_P1</code></td><td>✓</td><td>2512</td><td>2924</td><td>7.87</td><td>115.41</td><td>✓</td><td>7371</td><td>8500</td><td>8.22</td><td>729.83</td><td>✓</td><td>14982</td><td>17126</td><td>8.82</td><td>6804.37</td><td>—</td></tr>
<tr><td><code>A.GPS.FS1</code></td><td>✗</td><td>6165</td><td>7805</td><td>7.92</td><td>231.75</td><td>✗</td><td>8001</td><td>10102</td><td>8.01</td><td>472.60</td><td>✗</td><td>8031</td><td>10133</td><td>8.02</td><td>479.91</td><td>3</td></tr>
<tr><td><code>A.GPS.FS2</code></td><td>✓</td><td>1099</td><td>1260</td><td>7.62</td><td>74.54</td><td>✓</td><td>1206</td><td>1381</td><td>7.67</td><td>78.24</td><td>✓</td><td>1272</td><td>1454</td><td>7.72</td><td>74.39</td><td>—</td></tr>
<tr><td><code>A.CHUTE1</code></td><td>✗</td><td>521</td><td>585</td><td>7.77</td><td>10.24</td><td>✗</td><td>1574</td><td>1791</td><td>8.07</td><td>15.12</td><td>✗</td><td>3498</td><td>3973</td><td>8.07</td><td>36.12</td><td>3</td></tr>

</tbody>
</table>
